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

#include "spawn/Protocol.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "system/Error.hxx"
#include "util/Macros.hxx"
#include "util/PrintException.hxx"
#include "util/StringView.hxx"

#include <boost/crc.hpp>

#include <stdlib.h>
#include <stdio.h>

using namespace SpawnDaemon;

static UniqueSocketDescriptor
CreateConnectLocalSocket(const char *path)
{
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(AF_LOCAL, SOCK_SEQPACKET, 0))
		throw MakeErrno("Failed to create socket");

	{
		AllocatedSocketAddress address;
		address.SetLocal(path);
		if (!s.Connect(address))
			throw MakeErrno("Failed to bind");
	}

	return s;
}

static void
SendRequest(SocketDescriptor s, RequestCommand command,
	    ConstBuffer<struct iovec> payload)
{
	assert(payload.size < 4);

	size_t payload_size = 0;

	for (const auto &p : payload)
		payload_size += p.iov_len;

	const RequestHeader rh{uint16_t(payload_size), command};

	boost::crc_32_type crc;
	crc.reset();
	crc.process_bytes(&rh, sizeof(rh));

	for (const auto &p : payload)
		crc.process_bytes(p.iov_base, p.iov_len);

	const size_t padding_size = (-payload_size) & 3;
	static constexpr uint8_t padding[] = {0, 0, 0};
	crc.process_bytes(padding, padding_size);

	const DatagramHeader dh{MAGIC, crc.checksum()};

	struct iovec v[8];
	size_t nv = 0;

	v[nv++] = {const_cast<DatagramHeader *>(&dh), sizeof(dh)};
	v[nv++] = {const_cast<RequestHeader *>(&rh), sizeof(rh)};

	for (const auto &p : payload)
		v[nv++] = p;

	v[nv++] = {const_cast<uint8_t *>(padding), padding_size};

	struct msghdr h = {
		.msg_name = nullptr,
		.msg_namelen = 0,
		.msg_iov = v,
		.msg_iovlen = nv,
		.msg_control = nullptr,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	auto nbytes = sendmsg(s.Get(), &h, 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to send");
}

static void
SendMakeNamespaces(SocketDescriptor s, uint32_t flags, StringView name)
{
	const struct iovec payload[] = {
		{ &flags, sizeof(flags) },
		{ const_cast<char *>(name.data), name.size },
	};

	SendRequest(s, RequestCommand::MAKE_NAMESPACES,
		    ConstBuffer<struct iovec>(payload, ARRAY_SIZE(payload)));
}

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s NAME\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *name = argv[1];

	auto s = CreateConnectLocalSocket("@cm4all-spawn");
	SendMakeNamespaces(s, CLONE_NEWIPC, name);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
