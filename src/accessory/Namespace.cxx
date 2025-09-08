// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Namespace.hxx"
#include "spawn/Init.hxx"
#include "system/linux/clone3.h"
#include "system/Error.hxx"
#include "io/linux/ProcPid.hxx"
#include "io/Pipe.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

#include <sched.h>
#include <signal.h>

Namespace::~Namespace() noexcept
{
	if (pid_init > 0)
		kill(pid_init, SIGTERM);
}

/**
 * Clone a child process with the specified flags, and invoke the
 * specified function with a /proc/PID file descriptor.
 */
static auto
WithPipeChild(uint_least64_t flags, std::invocable<FileDescriptor> auto f)
{
	auto [r, w] = CreatePipe();

	const struct clone_args ca{
		.flags = CLONE_CLEAR_SIGHAND|flags,
		.exit_signal = SIGCHLD,
	};

	const pid_t pid = clone3(&ca, sizeof(ca));
	if (pid < 0)
		throw MakeErrno("clone3() failed");

	if (pid == 0) {
		w.Close();

		std::byte buffer[1];
		(void)r.Read(buffer);
		_exit(0);
	}

	r.Close();

	return f(OpenProcPid(pid));
}

FileDescriptor
Namespace::MakeIpc()
{
	if (ipc_ns.IsDefined())
		return ipc_ns;

	WithPipeChild(CLONE_NEWIPC, [this](FileDescriptor proc_pid){
		if (!ipc_ns.OpenReadOnly(proc_pid, "ns/ipc"))
			throw MakeErrno("Failed to open /proc/PID/ns/ipc");
	});

	return ipc_ns;
}

FileDescriptor
Namespace::MakePid()
{
	if (pid_ns.IsDefined())
		return pid_ns;

	pid_init = UnshareForkSpawnInit();

	try {
		const auto proc_pid = OpenProcPid(pid_init);

		if (!pid_ns.OpenReadOnly(proc_pid, "ns/pid"))
			throw MakeErrno("Failed to open /proc/PID/ns/pid");

		return pid_ns;
	} catch (...) {
		kill(std::exchange(pid_init, 0), SIGTERM);
		throw;
	}
}
