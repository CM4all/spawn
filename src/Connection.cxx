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
#include "Instance.hxx"
#include "spawn/Protocol.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"

#include <boost/crc.hpp>

using namespace SpawnDaemon;

SpawnConnection::SpawnConnection(Instance &_instance,
				 UniqueSocketDescriptor &&_fd,
				 SocketAddress)
	:instance(_instance),
	 peer_cred(_fd.GetPeerCredentials()),
	 listener(instance.GetEventLoop(), std::move(_fd), *this) {}

inline void
SpawnConnection::OnRequest(RequestCommand command, ConstBuffer<void> payload)
{
	printf("Received cmd=%u size=%zu\n", unsigned(command), payload.size);
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

	while (length > 0) {
		const auto &rh = *(const RequestHeader *)data;
		if (length < sizeof(rh))
			throw std::runtime_error("Malformed request in datagram");

		data = &rh + 1;
		length -= sizeof(rh);
		if (length < rh.size)
			throw std::runtime_error("Malformed request in datagram");
		length -= rh.size;

		OnRequest(rh.command, {data, rh.size});
	}

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
