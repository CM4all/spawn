/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "odbus/Connection.hxx"
#include "spawn/Systemd.hxx"
#include "event/Duration.hxx"
#include "util/PrintException.hxx"

#include <signal.h>
#include <unistd.h>

static constexpr auto dbus_reconnect_delay = ToEventDuration(std::chrono::seconds(5));

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 cgroup_state(CreateSystemdScope("spawn.scope",
					 "Process spawner helper daemon",
					 getpid(), true,
					 "system-cm4all.slice")),
	 dbus_watch(event_loop, *this),
	 dbus_reconnect_timer(event_loop, BIND_THIS_METHOD(ReconnectDBus)),
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

	/* this daemon should keep running even when DBus gets
	   restarted */
	dbus_connection_set_exit_on_disconnect(dbus_watch.GetConnection(),
					       false);

	agent.SetConnection(dbus_watch.GetConnection());
}

void
Instance::ReconnectDBus() noexcept
{
	try {
		ConnectDBus();
		fprintf(stderr, "Reconnected to DBus\n");
	} catch (...) {
		PrintException(std::current_exception());
		dbus_reconnect_timer.Add(dbus_reconnect_delay);
	}
}

void
Instance::OnDBusClosed() noexcept
{
	fprintf(stderr, "Connection to DBus lost\n");
	dbus_reconnect_timer.Add(dbus_reconnect_delay);
}
