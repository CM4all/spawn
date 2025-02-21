// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupAccounting.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Error.hxx"
#include "io/SmallTextFile.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"
#include "util/StringStrip.hxx"

using std::string_view_literals::operator""sv;

static CgroupCpuStat
ReadCgroupCpuStat(FileDescriptor cgroup_fd)
{
	CgroupCpuStat result;

	for (const std::string_view line : IterableSmallTextFile<4096>{FileAt{cgroup_fd, "cpu.stat"}}) {
		const auto [name, value_s] = Split(line, ' ');

		if (name == "usage_usec"sv) {
			if (auto value = ParseInteger<uint_least64_t>(value_s))
				result.total = std::chrono::microseconds(*value);
		} else if (name == "user_usec"sv) {
			if (auto value = ParseInteger<uint_least64_t>(value_s))
				result.user = std::chrono::microseconds(*value);
		} else if (name == "system_usec"sv) {
			if (auto value = ParseInteger<uint_least64_t>(value_s))
				result.system = std::chrono::microseconds(*value);
		}
	}

	return result;
}

CgroupResourceUsage
ReadCgroupResourceUsage(FileDescriptor cgroup_fd) noexcept
{
	// TODO: blkio

	CgroupResourceUsage result;

	try {
		result.cpu = ReadCgroupCpuStat(cgroup_fd);
	} catch (...) {
		PrintException(std::current_exception());
	}

	try {
		WithSmallTextFile<64>(FileAt{cgroup_fd, "memory.peak"}, [&result](std::string_view contents){
			if (auto value = ParseInteger<uint_least64_t>(StripRight(contents))) {
				result.memory_peak = *value;
				result.have_memory_peak = true;
			}
		});
	} catch (...) {
	}

	try {
		WithSmallTextFile<64>(FileAt{cgroup_fd, "pids.peak"}, [&result](std::string_view contents){
			if (auto value = ParseInteger<uint_least32_t>(StripRight(contents))) {
				result.pids_peak = *value;
				result.have_pids_peak = true;
			}
		});
	} catch (...) {
	}

	try {
		for (const std::string_view line : IterableSmallTextFile<4096>{FileAt{cgroup_fd, "pids.events"}}) {
			const auto [name, value_s] = Split(line, ' ');

			if (name == "max"sv) {
				if (auto value = ParseInteger<uint_least32_t>(value_s)) {
					result.pids_events_max = *value;
					result.have_pids_events_max = true;
				}
			}
		}
	} catch (...) {
	}

	return result;
}
