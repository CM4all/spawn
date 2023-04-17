// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Namespace.hxx"

#include <string>
#include <map>

class NamespaceMap {
	std::map<std::string, Namespace> map;

public:
	template<typename N>
	Namespace &operator[](N &&name) noexcept {
		return map[std::forward<N>(name)];
	}
};
