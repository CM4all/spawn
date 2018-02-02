/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef INSTANCE_HXX
#define INSTANCE_HXX

#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/TimerEvent.hxx"
#include "odbus/Watch.hxx"
#include "spawn/CgroupState.hxx"
#include "Agent.hxx"

class Instance final : ODBus::WatchManagerObserver  {
	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	const CgroupState cgroup_state;

	ODBus::WatchManager dbus_watch;
	TimerEvent dbus_reconnect_timer;

	SystemdAgent agent;

public:
	Instance();
	~Instance();

	EventLoop &GetEventLoop() {
		return event_loop;
	}

	void Dispatch() {
		event_loop.Dispatch();
	}

private:
	void OnExit();
	void OnReload(int);

	void ConnectDBus();
	void ReconnectDBus() noexcept;

	void OnSystemdAgentReleased(const char *path);

	/* virtual methods from ODBus::WatchManagerObserver */
	void OnDBusClosed() noexcept override;
};

#endif
