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

#include "Instance.hxx"
#include "Namespace.hxx"
#include "Scopes.hxx"
#include "UnifiedWatch.hxx"
#include "spawn/Systemd.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <signal.h>
#include <unistd.h>

static UniqueSocketDescriptor
CreateBindLocalSocket(const char *path)
{
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(AF_LOCAL, SOCK_SEQPACKET, 0))
		throw MakeErrno("Failed to create socket");

	s.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);

	{
		AllocatedSocketAddress address;
		address.SetLocal(path);
		if (!s.Bind(address))
			throw MakeErrno("Failed to bind");
	}

	if (!s.Listen(64))
		throw MakeErrno("Failed to listen");

	return s;
}

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 listener(event_loop, *this),
	 /* kludge: opening "/." so CgroupState contains file
	    descriptors to the root cgroup */
	 cgroup_state(CgroupState::FromProcess(0, "/.")),
	 defer_cgroup_delete(event_loop,
			     BIND_THIS_METHOD(OnDeferredCgroupDelete))
{
	listener.Listen(CreateBindLocalSocket("@cm4all-spawn"));

	if (!cgroup_state.IsEnabled())
		throw std::runtime_error("systemd cgroups are not available");

	if (const auto unified_mount = cgroup_state.GetUnifiedMountPath();
	    !unified_mount.empty()) {
		unified_cgroup_watch = std::make_unique<UnifiedCgroupWatch>(event_loop,
									    unified_mount.c_str(),
									    BIND_THIS_METHOD(OnSystemdAgentReleased));

		for (auto i = managed_scopes; *i != nullptr; ++i) {
			const char *relative_path = *i;
			if (*relative_path == '/')
				++relative_path;

			unified_cgroup_watch->AddCgroup(relative_path);
		}
	} else
		throw std::runtime_error("systemd unified cgroup is not available");

	shutdown_listener.Enable();
	sighup_event.Enable();
}

Instance::~Instance() noexcept = default;

void
Instance::OnExit() noexcept
{
	if (should_exit)
		return;

	should_exit = true;

	listener.RemoveEvent();
	listener.CloseAllConnections();

	shutdown_listener.Disable();
	sighup_event.Disable();

	unified_cgroup_watch.reset();
}

void
Instance::OnReload(int) noexcept
{
}
