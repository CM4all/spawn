// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "NamespaceMap.hxx"
#include "Namespace.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"

NamespaceMap::NamespaceMap(EventLoop &_event_loop) noexcept
	:event_loop(_event_loop) {}

NamespaceMap::~NamespaceMap() noexcept
{
	Clear();
}

inline std::size_t
NamespaceMap::Hash::operator()(std::string_view name) const noexcept
{
	return djb_hash(AsBytes(name));
}

std::string_view
NamespaceMap::GetKey::operator()(const Namespace &ns) const noexcept
{
	return ns.GetName();
}

void
NamespaceMap::Clear() noexcept
{
	map.clear_and_dispose(DeleteDisposer{});
}

Namespace &
NamespaceMap::operator[](std::string_view name) noexcept
{
	auto [i, inserted] = map.insert_check(name);

	if (inserted) {
		auto *ns = new Namespace(event_loop, name);
		i = map.insert_commit(i, *ns);
	}

	return *i;
}
