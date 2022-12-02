/*
 * Copyright 2017-2022 CM4all GmbH
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

#include "spawn/daemon/Builder.hxx"
#include "spawn/daemon/Protocol.hxx"
#include "spawn/daemon/Client.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/SendMessage.hxx"
#include "system/Error.hxx"
#include "util/CRC32.hxx"
#include "util/Macros.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"

#include <fmt/format.h>

#include <sched.h> // for CLONE_*
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/mount.h>

using namespace SpawnDaemon;

static void
SendMakeNamespaces(SocketDescriptor s, std::string_view name,
		   bool ipc_namespace, bool pid_namespace)
{
	DatagramBuilder b;

	const RequestHeader name_header{uint16_t(name.size()), RequestCommand::NAME};
	b.Append(name_header);
	b.AppendPadded(std::span{name});

	static constexpr RequestHeader ipc_namespace_header{0, RequestCommand::IPC_NAMESPACE};
	if (ipc_namespace)
		b.Append(ipc_namespace_header);

	static constexpr RequestHeader pid_namespace_header{0, RequestCommand::PID_NAMESPACE};
	if (pid_namespace)
		b.Append(pid_namespace_header);

	SendMessage(s, b.Finish(), 0);
}

static void
SetNs(std::span<const uint32_t> nstypes,
      std::vector<UniqueFileDescriptor> &&fds)
{
	assert(nstypes.size() == fds.size());

	auto i = fds.begin();
	for (auto nstype : nstypes) {
		if (setns(i->Get(), nstype) < 0)
			throw FormatErrno("setns(0x%x) failed", nstype);

		++i;
	}
}

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fmt::print(stderr, "Usage: {} NAME\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *name = argv[1];

	auto s = Connect();
	SendMakeNamespaces(s, name, true, true);

	ReceiveMessageBuffer<1024, 256> buffer;
	auto response = ReceiveMessage(s, buffer, 0);
	s.Close();
	auto payload = response.payload;
	const auto &dh = *(const DatagramHeader *)(const void *)payload.data();
	if (payload.size() < sizeof(dh))
		throw std::runtime_error("Malformed response");

	payload = payload.subspan(sizeof(dh));

	if (dh.crc != CRC32(payload))
		throw std::runtime_error("Bad CRC");

	const auto &rh = *(const ResponseHeader *)(const void *)payload.data();
	if (payload.size() < sizeof(rh))
		throw std::runtime_error("Malformed response");

	payload = payload.subspan(sizeof(rh));

	if (payload.size() < rh.size)
		throw std::runtime_error("Malformed response");

	switch (rh.command) {
	case ResponseCommand::ERROR:
		fmt::print(stderr, "Server error: {}\n",
			   ToStringView(payload));
		return EXIT_FAILURE;

	case ResponseCommand::NAMESPACE_HANDLES:
		if (rh.size != (size_t)std::distance(response.fds.begin(),
						     response.fds.end()) * sizeof(uint32_t))
			throw std::runtime_error("Malformed NAMESPACE_HANDLES payload");

		SetNs(FromBytesStrict<const uint32_t>(payload),
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
