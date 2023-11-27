// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

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

	const CgroupState cgroup_state;

	std::unique_ptr<UnifiedCgroupWatch> unified_cgroup_watch;

	std::unique_ptr<LuaAccounting> lua_accounting;

	std::set<std::string> cgroup_delete_queue;
	FineTimerEvent defer_cgroup_delete;

public:
	Instance();
	~Instance() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	void Run() noexcept {
		event_loop.Run();
	}

private:
	void OnExit() noexcept;
	void OnReload(int) noexcept;

	void OnCgroupEmpty(const char *path) noexcept;
	void OnDeferredCgroupDelete() noexcept;
};
