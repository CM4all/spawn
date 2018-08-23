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

#include "Namespace.hxx"
#include "spawn/Init.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <sched.h>
#include <signal.h>

Namespace::~Namespace()
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
