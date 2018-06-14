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

#include "Instance.hxx"
#include "Namespace.hxx"
#include "Scopes.hxx"
#include "UnifiedWatch.hxx"
#include "Agent.hxx"
#include "odbus/Connection.hxx"
#include "spawn/Systemd.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <signal.h>
#include <unistd.h>

static constexpr auto dbus_reconnect_delay = std::chrono::seconds(5);

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
	 cgroup_state(CreateSystemdScope("spawn.scope",
					 "Process spawner helper daemon",
					 {},
					 getpid(), true,
					 "system-cm4all.slice")),
	 dbus_watch(event_loop, *this),
	 dbus_reconnect_timer(event_loop, BIND_THIS_METHOD(ReconnectDBus))
{
	listener.Listen(CreateBindLocalSocket("@cm4all-spawn"));

	ConnectDBus();

	if (!cgroup_state.IsEnabled())
		throw std::runtime_error("systemd cgroups are not available");

	if (HasUnifiedCgroups()) {
		unified_cgroup_watch = std::make_unique<UnifiedCgroupWatch>(event_loop,
									    BIND_THIS_METHOD(OnSystemdAgentReleased));

		for (auto i = managed_scopes; *i != nullptr; ++i) {
			const char *relative_path = *i;
			if (*relative_path == '/')
				++relative_path;

			unified_cgroup_watch->AddCgroup(relative_path);
		}
	} else
		agent = std::make_unique<SystemdAgent>(BIND_THIS_METHOD(OnSystemdAgentReleased));

	shutdown_listener.Enable();
	sighup_event.Enable();
}

Instance::~Instance()
{
}

void
Instance::OnExit()
{
	if (should_exit)
		return;

	should_exit = true;

	listener.RemoveEvent();
	listener.CloseAllConnections();

	dbus_watch.Shutdown();
	shutdown_listener.Disable();
	sighup_event.Disable();
}

void
Instance::OnReload(int)
{
}

void
Instance::ConnectDBus()
{
	dbus_watch.SetConnection(ODBus::Connection::GetSystem());

	/* this daemon should keep running even when DBus gets
	   restarted */
	dbus_connection_set_exit_on_disconnect(dbus_watch.GetConnection(),
					       false);

	if (agent)
		agent->SetConnection(dbus_watch.GetConnection());
}

void
Instance::ReconnectDBus() noexcept
{
	try {
		ConnectDBus();
		fprintf(stderr, "Reconnected to DBus\n");
	} catch (...) {
		PrintException(std::current_exception());
		dbus_reconnect_timer.Schedule(dbus_reconnect_delay);
	}
}

void
Instance::OnDBusClosed() noexcept
{
	fprintf(stderr, "Connection to DBus lost\n");
	dbus_reconnect_timer.Schedule(dbus_reconnect_delay);
}
