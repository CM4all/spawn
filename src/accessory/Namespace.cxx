// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Namespace.hxx"
#include "spawn/Init.hxx"
#include "system/Error.hxx"
#include "io/linux/ProcPid.hxx"
#include "io/UniqueFileDescriptor.hxx"

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

	pid_init = UnshareForkSpawnInit();

	try {
		const auto proc_pid = OpenProcPid(pid_init);

		/* note: this requires Linux 4.12 */
		if (!pid_ns.OpenReadOnly(proc_pid, "ns/pid"))
			throw MakeErrno("Failed to open /proc/PID/ns/pid");

		return pid_ns;
	} catch (...) {
		kill(std::exchange(pid_init, 0), SIGTERM);
		throw;
	}
}
