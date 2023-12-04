// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>
#include <string>
#include <span>

namespace SpawnAccessory { enum class RequestCommand : uint16_t; }

struct SpawnRequest {
	std::string name;

	bool ipc_namespace = false;
	bool pid_namespace = false;

	bool IsNamespace() const noexcept {
		return ipc_namespace || pid_namespace;
	}

	void Apply(SpawnAccessory::RequestCommand command,
		   std::span<const std::byte> payload);
};
