// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Namespace.hxx"

#include <string>
#include <map>

class NamespaceMap {
	std::map<std::string, Namespace, std::less<>> map;

public:
	Namespace &operator[](std::string_view name) noexcept {
		if (auto i = map.find(name); i != map.end())
			return i->second;

		return map.emplace(std::piecewise_construct,
				   std::forward_as_tuple(name),
				   std::forward_as_tuple()).first->second;
	}
};
