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

#include "Builder.hxx"
#include "spawn/Protocol.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/SendMessage.hxx"
#include "system/Error.hxx"
#include "util/Macros.hxx"
#include "util/PrintException.hxx"
#include "util/StringView.hxx"
#include "util/StaticArray.hxx"

#include <boost/crc.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/mount.h>

using namespace SpawnDaemon;

static UniqueSocketDescriptor
CreateConnectLocalSocket(const char *path)
{
	UniqueSocketDescriptor s;
	if (!s.Create(AF_LOCAL, SOCK_SEQPACKET, 0))
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
SendMakeNamespaces(SocketDescriptor s, StringView name,
		   bool ipc_namespace, bool pid_namespace)
{
	DatagramBuilder b;

	const RequestHeader name_header{uint16_t(name.size), RequestCommand::NAME};
	b.Append(name_header);
	b.AppendPadded(name.ToVoid());

	static constexpr RequestHeader ipc_namespace_header{0, RequestCommand::IPC_NAMESPACE};
	if (ipc_namespace)
		b.Append(ipc_namespace_header);

	static constexpr RequestHeader pid_namespace_header{0, RequestCommand::PID_NAMESPACE};
	if (pid_namespace)
		b.Append(pid_namespace_header);

	SendMessage(s, b.Finish(), 0);
}

static void
SetNs(ConstBuffer<uint32_t> nstypes,
      std::forward_list<UniqueFileDescriptor> &&fds)
{
	assert(nstypes.size == (size_t)std::distance(fds.begin(), fds.end()));

	for (auto nstype : nstypes) {
		if (setns(fds.front().Get(), nstype) < 0)
			throw FormatErrno("setns(0x%x) failed", nstype);

		fds.pop_front();
	}
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
	SendMakeNamespaces(s, name, true, true);

	ReceiveMessageBuffer<1024, 256> buffer;
	auto response = ReceiveMessage(s, buffer, 0);
	s.Close();
	auto payload = response.payload;
	const auto &dh = *(const DatagramHeader *)payload.data;
	if (payload.size < sizeof(dh))
		throw std::runtime_error("Malformed response");

	payload.data = &dh + 1;
	payload.size -= sizeof(dh);

	{
		boost::crc_32_type crc;
		crc.reset();
		crc.process_bytes(payload.data, payload.size);
		if (dh.crc != crc.checksum())
			throw std::runtime_error("Bad CRC");
	}

	const auto &rh = *(const ResponseHeader *)payload.data;
	if (payload.size < sizeof(rh))
		throw std::runtime_error("Malformed response");

	payload.data = &rh + 1;
	payload.size -= sizeof(rh);

	if (payload.size < rh.size)
		throw std::runtime_error("Malformed response");

	switch (rh.command) {
	case ResponseCommand::ERROR:
		throw std::runtime_error(std::string((const char *)payload.data,
						     rh.size));

	case ResponseCommand::NAMESPACE_HANDLES:
		if (rh.size != (size_t)std::distance(response.fds.begin(),
						     response.fds.end()) * sizeof(uint32_t))
			throw std::runtime_error("Malformed NAMESPACE_HANDLES payload");

		SetNs(ConstBuffer<uint32_t>::FromVoid({payload.data, rh.size}),
		      std::move(response.fds));

		{
			const auto pid = fork();
			if (pid < 0)
				throw MakeErrno("fork() failed");

			if (pid == 0) {
				if (unshare(CLONE_NEWNS) < 0) {
					perror("unshare(CLONE_NEWNS) failed");
					_exit(EXIT_FAILURE);
				}

				if (mount(nullptr, "/", nullptr,
					  MS_SLAVE|MS_REC, nullptr) < 0) {
					perror("mount(MS_SLAVE) failed");
					_exit(EXIT_FAILURE);
				}

				if (mount("proc", "/proc", "proc",
					  MS_NOEXEC|MS_NOSUID|MS_NODEV,
					  nullptr) < 0) {
					perror("mount(/proc) failed");
					_exit(EXIT_FAILURE);
				}

				execl("/bin/sh", "sh", nullptr);
				throw MakeErrno("Failed to execute a shell");
			}

			int status;
			wait(&status);
		}
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
