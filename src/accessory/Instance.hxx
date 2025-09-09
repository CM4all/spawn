// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Listener.hxx"
#include "NamespaceMap.hxx"
#include "spawn/ZombieReaper.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"

#include <forward_list>

class Instance final {
	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;
	ZombieReaper zombie_reaper{event_loop};

	std::forward_list<SpawnListener> listeners;

	NamespaceMap namespaces{event_loop};

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
};
