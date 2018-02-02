/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "odbus/Connection.hxx"
#include "spawn/Systemd.hxx"

#include <signal.h>
#include <unistd.h>

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 cgroup_state(CreateSystemdScope("spawn.scope",
					 "Process spawner helper daemon",
					 getpid(), true,
					 "system-cm4all.slice")),
	 dbus_watch(event_loop, ODBus::Connection::GetSystem()),
	 agent(BIND_THIS_METHOD(OnSystemdAgentReleased))
{
	ConnectDBus();

	if (!cgroup_state.IsEnabled())
		throw std::runtime_error("systemd cgroups are not available");

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
	agent.SetConnection(dbus_watch.GetConnection());
}
