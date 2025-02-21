// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>
#include <cstdint>

class FileDescriptor;

struct CgroupCpuStat {
	using Duration = std::chrono::duration<double>;

	Duration total{-1}, user{-1}, system{-1};
};

struct CgroupResourceUsage {
	CgroupCpuStat cpu;

	uint_least64_t memory_peak;

	uint_least32_t pids_peak;

	bool have_memory_peak = false;

	bool have_pids_peak = false;
};

[[gnu::pure]]
CgroupResourceUsage
ReadCgroupResourceUsage(FileDescriptor cgroup_fd) noexcept;
