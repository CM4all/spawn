// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <span>

namespace SpawnAccessory { enum class RequestCommand : uint16_t; }

struct SpawnRequest {
	std::string_view name;

	bool ipc_namespace = false;
	bool pid_namespace = false;
	bool user_namespace = false;
	std::string user_namespace_payload;
	bool lease_pipe = false;

	bool IsNamespace() const noexcept {
		return ipc_namespace || pid_namespace || user_namespace;
	}

	bool IsLeasePipe() const noexcept {
		return lease_pipe;
	}

	void Apply(SpawnAccessory::RequestCommand command,
		   std::span<const std::byte> payload);
};
