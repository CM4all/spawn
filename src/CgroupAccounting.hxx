// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>
#include <cstdint>

struct CgroupState;

struct CgroupCpuStat {
	using Duration = std::chrono::duration<double>;

	Duration total{-1}, user{-1}, system{-1};
};

struct CgroupResourceUsage {
	CgroupCpuStat cpu;

	uint64_t memory_max_usage;

	bool have_memory_max_usage = false;
};

[[gnu::pure]]
CgroupResourceUsage
ReadCgroupResourceUsage(const CgroupState &state,
			const char *relative_path) noexcept;
