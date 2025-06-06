// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Request.hxx"
#include "spawn/accessory/Protocol.hxx"
#include "util/SpanCast.hxx"

#include <fmt/format.h>

#include <stdexcept>

using namespace SpawnAccessory;

static std::string
CheckNonEmptyASCII(std::string_view payload)
{
	if (payload.empty())
		throw std::runtime_error("Empty string");

	for (char ch : payload)
		if ((signed char)ch < 0x20)
			throw std::runtime_error("Malformed string");

	return std::string{payload};
}

static std::string
CheckNonEmptyASCII(std::span<const std::byte> payload)
{
	return CheckNonEmptyASCII(ToStringView(payload));
}

void
SpawnRequest::Apply(RequestCommand command, std::span<const std::byte> payload)
{
	fmt::print("Received cmd={} size={}\n",
		   unsigned(command), payload.size());

	switch (command) {
	case RequestCommand::NOP:
		break;

	case RequestCommand::NAME:
		if (!name.empty())
			throw std::runtime_error("Duplicate NAME");

		name = CheckNonEmptyASCII(payload);
		break;

	case RequestCommand::IPC_NAMESPACE:
		if (ipc_namespace)
			throw std::runtime_error("Duplicate IPC_NAMESPACE");

		if (!payload.empty())
			throw std::runtime_error("Malformed IPC_NAMESPACE");

		ipc_namespace = true;
		break;

	case RequestCommand::PID_NAMESPACE:
		if (pid_namespace)
			throw std::runtime_error("Duplicate PID_NAMESPACE");

		if (!payload.empty())
			throw std::runtime_error("Malformed PID_NAMESPACE");

		pid_namespace = true;
		break;
	}
}
