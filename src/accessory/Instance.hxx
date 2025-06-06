// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Listener.hxx"
#include "NamespaceMap.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"

#include <forward_list>

class Instance final {
	EventLoop event_loop;

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;

	std::forward_list<SpawnListener> listeners;

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
};
