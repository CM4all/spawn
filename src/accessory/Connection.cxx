// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Connection.hxx"
#include "Request.hxx"
#include "Instance.hxx"
#include "spawn/accessory/Protocol.hxx"
#include "spawn/accessory/Builder.hxx"
#include "net/SendMessage.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "io/Iovec.hxx"
#include "system/Error.hxx"
#include "util/CRC32.hxx"
#include "util/PrintException.hxx"
#include "util/Exception.hxx"
#include "util/SpanCast.hxx"
#include "util/StaticVector.hxx"

#include <forward_list>

#include <assert.h>
#include <sched.h> // for CLONE_*

using namespace SpawnAccessory;

SpawnConnection::SpawnConnection(Instance &_instance,
				 UniqueSocketDescriptor &&_fd,
				 SocketAddress)
	:instance(_instance),
	 listener(instance.GetEventLoop(), std::move(_fd), *this) {}

void
SpawnConnection::SendError(std::string_view msg)
{
	DatagramBuilder b;

	const ResponseHeader rh{uint16_t(msg.size()), ResponseCommand::ERROR};
	b.Append(rh);
	b.AppendPadded(AsBytes(msg));

	SendMessage(listener.GetSocket(), b.Finish(),
		    MSG_DONTWAIT|MSG_NOSIGNAL);
}

inline void
SpawnConnection::OnMakeNamespaces(SpawnRequest &&request)
{
	if (request.name.empty())
		throw std::runtime_error("No NAME");

	auto &ns = instance.GetNamespaces()[request.name];

	StaticVector<uint32_t, 8> response_payload;
	std::forward_list<UniqueFileDescriptor> response_fds;

	struct iovec v[3];

	MessageHeader msg = std::span<const struct iovec>{v};
	ScmRightsBuilder<8> srb(msg);

	try {
		if (request.ipc_namespace) {
			srb.push_back(ns.MakeIpc().Get());
			response_payload.push_back(CLONE_NEWIPC);
		}

		if (request.pid_namespace) {
			srb.push_back(ns.MakePid().Get());
			response_payload.push_back(CLONE_NEWPID);
		}

		if (request.user_namespace) {
			srb.push_back(ns.MakeUser(request.user_namespace_payload).Get());
			response_payload.push_back(CLONE_NEWUSER);
		}
	} catch (...) {
		PrintException(std::current_exception());
		SendError(GetFullMessage(std::current_exception()));
		return;
	}

	assert(!response_payload.empty());

	const auto response_payload_span = std::as_bytes(std::span{response_payload});

	v[2] = MakeIovec(response_payload_span);

	srb.Finish(msg);

	const ResponseHeader rh{uint16_t(response_payload_span.size()), ResponseCommand::NAMESPACE_HANDLES};
	v[1] = MakeIovec(ReferenceAsBytes(rh));

	CRC32State crc;
	crc.Update(ReferenceAsBytes(rh));
	crc.Update(response_payload_span);

	const DatagramHeader dh{MAGIC, crc.Finish()};
	v[0] = MakeIovec(ReferenceAsBytes(dh));

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
SpawnConnection::OnUdpDatagram(std::span<const std::byte> payload,
			       std::span<UniqueFileDescriptor>,
			       SocketAddress, int)
try {
	if (payload.empty()) {
		delete this;
		return false;
	}

	const auto &dh = *(const DatagramHeader *)(const void *)payload.data();
	if (payload.size() < sizeof(dh) || dh.magic != MAGIC)
		throw std::runtime_error("Malformed datagram");

	payload = payload.subspan(sizeof(dh));

	if (dh.crc != CRC32(payload))
		throw std::runtime_error("Bad CRC");

	SpawnRequest request;

	while (!payload.empty()) {
		const auto &rh = *(const RequestHeader *)(const void *)payload.data();
		if (payload.size() < sizeof(rh))
			throw std::runtime_error("Malformed request in datagram");

		payload = payload.subspan(sizeof(rh));

		const size_t payload_size = rh.size;
		const size_t padded_size = (payload_size + 3) & (~3u);

		if (payload.size() < padded_size)
			throw std::runtime_error("Malformed request in datagram");

		request.Apply(rh.command, payload.first(payload_size));

		payload = payload.subspan(padded_size);
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
