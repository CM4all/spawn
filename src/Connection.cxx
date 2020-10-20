/*
 * Copyright 2017-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Connection.hxx"
#include "Request.hxx"
#include "Instance.hxx"
#include "spawn/daemon/Protocol.hxx"
#include "spawn/daemon/Builder.hxx"
#include "net/SendMessage.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"
#include "util/PrintException.hxx"
#include "util/Exception.hxx"
#include "util/StaticArray.hxx"

#include <boost/crc.hpp>

#include <assert.h>

using namespace SpawnDaemon;

SpawnConnection::SpawnConnection(Instance &_instance,
				 UniqueSocketDescriptor &&_fd,
				 SocketAddress)
	:instance(_instance),
	 listener(instance.GetEventLoop(), std::move(_fd), *this) {}

void
SpawnConnection::SendError(StringView msg)
{
	DatagramBuilder b;

	const ResponseHeader rh{uint16_t(msg.size), ResponseCommand::ERROR};
	b.Append(rh);
	b.AppendPadded(msg.ToVoid());

	SendMessage(listener.GetSocket(), b.Finish(),
		    MSG_DONTWAIT|MSG_NOSIGNAL);
}

inline void
SpawnConnection::OnMakeNamespaces(SpawnRequest &&request)
{
	if (request.name.empty())
		throw std::runtime_error("No NAME");

	int flags = 0;
	if (request.ipc_namespace)
		flags |= CLONE_NEWIPC;
	if (request.pid_namespace)
		flags |= CLONE_NEWPID;

	auto &ns = instance.GetNamespaces()[std::move(request.name)];

	StaticArray<uint32_t, 8> response_payload;
	std::forward_list<UniqueFileDescriptor> response_fds;

	struct iovec v[3];

	MessageHeader msg = ConstBuffer<struct iovec>(v);
	ScmRightsBuilder<8> srb(msg);

	try {
		if (flags & CLONE_NEWIPC) {
			srb.push_back(ns.MakeIpc().Get());
			response_payload.push_back(CLONE_NEWIPC);
		}

		if (flags & CLONE_NEWPID) {
			srb.push_back(ns.MakePid().Get());
			response_payload.push_back(CLONE_NEWPID);
		}
	} catch (...) {
		PrintException(std::current_exception());
		const auto error = GetFullMessage(std::current_exception());
		SendError({error.data(), error.size()});
		return;
	}

	assert(!response_payload.empty());

	const size_t response_payload_size = response_payload.size() * sizeof(response_payload.front());
	v[2].iov_base = &response_payload.front();
	v[2].iov_len = response_payload_size;

	srb.Finish(msg);

	const ResponseHeader rh{uint16_t(response_payload_size), ResponseCommand::NAMESPACE_HANDLES};
	v[1].iov_base = const_cast<ResponseHeader *>(&rh);
	v[1].iov_len = sizeof(rh);

	boost::crc_32_type crc;
	crc.reset();
	crc.process_bytes(&rh, sizeof(rh));
	crc.process_bytes(&response_payload.front(), response_payload_size);

	const DatagramHeader dh{MAGIC, crc.checksum()};
	v[0].iov_base = const_cast<DatagramHeader *>(&dh);
	v[0].iov_len = sizeof(dh);

	SendMessage(listener.GetSocket(), msg,
		    MSG_DONTWAIT|MSG_NOSIGNAL);
}

inline void
SpawnConnection::OnRequest(SpawnRequest &&request)
{
	if (request.IsNamespace())
		OnMakeNamespaces(std::move(request));
}

bool
SpawnConnection::OnUdpDatagram(const void *data, size_t length,
			       SocketAddress, int)
try {
	if (length == 0) {
		delete this;
		return false;
	}

	const auto &dh = *(const DatagramHeader *)data;
	if (length < sizeof(dh) || dh.magic != MAGIC)
		throw std::runtime_error("Malformed datagram");

	data = &dh + 1;
	length -= sizeof(dh);

	{
		boost::crc_32_type crc;
		crc.reset();
		crc.process_bytes(data, length);
		if (dh.crc != crc.checksum())
			throw std::runtime_error("Bad CRC");
	}

	SpawnRequest request;

	while (length > 0) {
		const auto &rh = *(const RequestHeader *)data;
		if (length < sizeof(rh))
			throw std::runtime_error("Malformed request in datagram");

		data = &rh + 1;
		length -= sizeof(rh);

		const size_t payload_size = rh.size;
		const size_t padded_size = (payload_size + 3) & (~3u);

		if (length < padded_size)
			throw std::runtime_error("Malformed request in datagram");
		length -= padded_size;

		request.Apply(rh.command, {data, payload_size});

		data = (const uint8_t *)data + padded_size;
	}

	OnRequest(std::move(request));

	return true;
} catch (...) {
	PrintException(std::current_exception());
	delete this;
	return false;
}

void
SpawnConnection::OnUdpError(std::exception_ptr ep) noexcept
{
	PrintException(ep);
	delete this;
}
