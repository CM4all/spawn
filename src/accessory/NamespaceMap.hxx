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
	template<typename N>
	Namespace &operator[](N &&name) noexcept {
		return map[std::forward<N>(name)];
	}
};
