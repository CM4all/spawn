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

	uint_least32_t memory_events_high, memory_events_max, memory_events_oom;

	uint_least32_t pids_peak, pids_events_max;

	bool have_memory_peak = false;

	bool have_memory_events_high = false, have_memory_events_max = false;
	bool have_memory_events_oom = false;

	bool have_pids_peak = false, have_pids_events_max = false;
};

[[gnu::pure]]
CgroupResourceUsage
ReadCgroupResourceUsage(FileDescriptor cgroup_fd) noexcept;
