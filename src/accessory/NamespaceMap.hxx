// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>
#include <map>

class EventLoop;
class Namespace;

class NamespaceMap {
	EventLoop &event_loop;

	std::map<std::string, Namespace, std::less<>> map;

public:
	explicit NamespaceMap(EventLoop &_event_loop) noexcept;
	~NamespaceMap() noexcept;

	void Clear() noexcept;

	Namespace &operator[](std::string_view name) noexcept;
};
