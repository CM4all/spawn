// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef INSTANCE_HXX
#define INSTANCE_HXX

#include "Listener.hxx"
#include "NamespaceMap.hxx"
#include "lua/State.hxx"
#include "lua/ValuePtr.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/FineTimerEvent.hxx"
#include "spawn/CgroupState.hxx"

#include <memory>
#include <set>
#include <string>

class UnifiedCgroupWatch;
class LuaAccounting;

class Instance final {
	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	SpawnListener listener;

	const CgroupState cgroup_state;

	std::unique_ptr<UnifiedCgroupWatch> unified_cgroup_watch;

	std::unique_ptr<LuaAccounting> lua_accounting;

	std::set<std::string> cgroup_delete_queue;
	FineTimerEvent defer_cgroup_delete;

	NamespaceMap namespaces;

public:
	Instance();
	~Instance() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	void Run() noexcept {
		event_loop.Run();
	}

	NamespaceMap &GetNamespaces() noexcept {
		return namespaces;
	}

private:
	void OnExit() noexcept;
	void OnReload(int) noexcept;

	void OnSystemdAgentReleased(const char *path) noexcept;
	void OnDeferredCgroupDelete() noexcept;
};

#endif
