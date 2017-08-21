/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "odbus/Connection.hxx"

#include <signal.h>

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 dbus_watch(event_loop, ODBus::Connection::GetSystem())
{
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
