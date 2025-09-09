// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/IntrusiveHashSet.hxx"

#include <string>

class EventLoop;
class Namespace;

class NamespaceMap {
	EventLoop &event_loop;

	struct Hash {
		[[gnu::pure]]
		std::size_t operator()(std::string_view name) const noexcept;
	};

	struct GetKey {
		[[gnu::pure]]
		std::string_view operator()(const Namespace &ns) const noexcept;
	};

	IntrusiveHashSet<Namespace, 1024,
			 IntrusiveHashSetOperators<Namespace, GetKey, Hash,
						   std::equal_to<std::string_view>>> map;

public:
	explicit NamespaceMap(EventLoop &_event_loop) noexcept;
	~NamespaceMap() noexcept;

	void Clear() noexcept;

	Namespace &operator[](std::string_view name) noexcept;
};
