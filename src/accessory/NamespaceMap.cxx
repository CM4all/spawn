// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "NamespaceMap.hxx"
#include "Namespace.hxx"

NamespaceMap::NamespaceMap(EventLoop &_event_loop) noexcept
	:event_loop(_event_loop) {}

NamespaceMap::~NamespaceMap() noexcept = default;

void
NamespaceMap::Clear() noexcept
{
	map.clear();
}

Namespace &
NamespaceMap::operator[](std::string_view name) noexcept
{
	if (auto i = map.find(name); i != map.end())
		return i->second;

	return map.emplace(std::piecewise_construct,
			   std::forward_as_tuple(name),
			   std::forward_as_tuple(event_loop)).first->second;
}
