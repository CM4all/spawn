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
	 /* obtain cgroup information from the init process (=
	    systemd); we know it exists, and we know it has proper
	    cgroups, while this process may or may not be set up
	    properly */
	 cgroup_state(CreateSystemdScope("spawn.scope",
					 "Process spawner helper daemon",
					 getpid(), true,
					 "system-cm4all.slice")),
	 dbus_watch(event_loop, ODBus::Connection::GetSystem()),
	 agent(BIND_THIS_METHOD(OnSystemdAgentReleased))
{
	if (!cgroup_state.IsEnabled())
		throw std::runtime_error("systemd cgroups are not available");

	shutdown_listener.Enable();
	sighup_event.Enable();

	agent.SetConnection(ODBus::Connection::GetSystem());
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
