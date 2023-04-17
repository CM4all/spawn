// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Namespace.hxx"
#include "spawn/Init.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <sched.h>
#include <signal.h>

Namespace::~Namespace() noexcept
{
	if (pid_init > 0)
		kill(pid_init, SIGTERM);
}

FileDescriptor
Namespace::MakeIpc()
{
	if (ipc_ns.IsDefined())
		return ipc_ns;

	if (unshare(CLONE_NEWIPC) < 0)
		throw MakeErrno("unshare(CLONE_NEWIPC) failed");

	if (!ipc_ns.OpenReadOnly("/proc/self/ns/ipc"))
		throw MakeErrno("open(\"/proc/self/ns/ipc\") failed");

	return ipc_ns;
}

FileDescriptor
Namespace::MakePid()
{
	if (pid_ns.IsDefined())
		return pid_ns;

	/* automatically restore the old PID namespace, or else the
	   next unshare() call will fail with EINVAL (generated by
	   copy_pid_ns() in kernel/pid_namespace.c) */
	UniqueFileDescriptor old;
	if (!old.OpenReadOnly("/proc/self/ns/pid"))
		throw MakeErrno("Failed to open current PID namespace");

	AtScopeExit(&old) {
		setns(old.Get(), CLONE_NEWPID);
	};

	pid_init = UnshareForkSpawnInit();

	try {
		/* note: this requires Linux 4.12 */
		if (!pid_ns.OpenReadOnly("/proc/self/ns/pid_for_children"))
			throw MakeErrno("open(\"/proc/self/ns/pid_for_children\") failed");

		return pid_ns;
	} catch (...) {
		kill(std::exchange(pid_init, 0), SIGTERM);
		throw;
	}
}
