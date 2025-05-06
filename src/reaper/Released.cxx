// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Scopes.hxx"
#include "CgroupAccounting.hxx"
#include "LAccounting.hxx"
#include "time/ISO8601.hxx"
#include "time/StatxCast.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringCompare.hxx"

#include <fmt/format.h>

#include <fcntl.h> // for AT_REMOVEDIR
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

using std::string_view_literals::operator""sv;

static const char *
GetManagedSuffix(const char *path)
{
	for (auto i = managed_scopes; *i != nullptr; ++i) {
		const char *suffix = StringAfterPrefix(path, *i);
		if (suffix != nullptr)
			return suffix;
	}

	return nullptr;
}

static void
CollectCgroupStats(const char *suffix,
		   const std::chrono::system_clock::time_point btime,
		   const CgroupResourceUsage &u)
{
	char buffer[4096], *p = buffer;

	if (btime != std::chrono::system_clock::time_point{})
		p = fmt::format_to(p, " since={}"sv, FormatISO8601(btime).c_str());

	if (u.cpu.user.count() >= 0 || u.cpu.system.count() >= 0) {
		const auto user = std::max(u.cpu.user.count(), 0.);
		const auto system = std::max(u.cpu.system.count(), 0.);
		const auto total = u.cpu.total.count() >= 0
			? u.cpu.total.count()
			: user + system;

		p = fmt::format_to(p, " cpu={}s/{}s/{}s", total, user, system);
	} else if (u.cpu.total.count() >= 0) {
		p = fmt::format_to(p, " cpu={}s", u.cpu.total.count());
	}

	if (u.have_memory_peak) {
		static constexpr uint_least64_t MEGA = 1024 * 1024;

		p = fmt::format_to(p, " memory={}M",
				   (u.memory_peak + MEGA / 2 - 1) / MEGA);
	}

	if ((u.have_memory_events_high && u.memory_events_high > 0) ||
	    (u.have_memory_events_max && u.memory_events_max > 0)) {
		const auto high = u.have_memory_events_high
			? u.memory_events_high
			: 0;
		const auto max = u.have_memory_events_max
			? u.memory_events_max
			: 0;

		p = fmt::format_to(p, " reclaim={}", high + max);
	}

	if (u.have_memory_events_oom && u.memory_events_oom > 0)
		p = fmt::format_to(p, " oom={}", u.memory_events_oom);

	if (u.have_pids_peak)
		p = fmt::format_to(p, " procs={}", u.pids_peak);

	if (u.have_pids_forks)
		p = fmt::format_to(p, " forks={}", u.pids_forks);

	if (u.have_pids_events_max && u.pids_events_max > 0)
		p = fmt::format_to(p, " procs_rejected={}", u.pids_events_max);

	if (p > buffer)
		fmt::print(stderr, "{}:{}\n", suffix,
			   std::string_view{buffer, p});
}

static void
DestroyCgroup(const FileDescriptor root_cgroup, const char *relative_path) noexcept
{
	assert(*relative_path == '/');
	assert(relative_path[1] != 0);

	if (unlinkat(root_cgroup.Get(), relative_path + 1,
		     AT_REMOVEDIR) < 0 &&
	    errno != ENOENT)
		fmt::print(stderr, "Failed to delete '{}': {}\n",
			   relative_path, strerror(errno));
}

void
Instance::OnCgroupEmpty(const char *path) noexcept
{
	const char *suffix = GetManagedSuffix(path);
	if (suffix == nullptr)
		return;

	UniqueFileDescriptor cgroup_fd;
	(void)cgroup_fd.Open(root_cgroup, path + 1, O_DIRECTORY|O_RDONLY);

	std::chrono::system_clock::time_point btime;

	if (cgroup_fd.IsDefined()) {
		struct statx stx;
		if (statx(cgroup_fd.Get(), "", AT_EMPTY_PATH|AT_STATX_FORCE_SYNC,
			  STATX_BTIME, &stx) == 0) {
			if (stx.stx_mask & STATX_BTIME)
				btime = ToSystemTimePoint(stx.stx_btime);
		}
	}

	// TODO read resource usage right before the cgroup actually gets deleted
	const auto u = cgroup_fd.IsDefined()
		? ReadCgroupResourceUsage(cgroup_fd)
		: CgroupResourceUsage{};

	CollectCgroupStats(suffix, btime, u);

	if (lua_accounting)
		lua_accounting->InvokeCgroupReleased(std::move(cgroup_fd), path, u);

	/* defer the deletion, because unpopulated children of this
	   cgroup may still exist; this deferral attempts to get the
	   ordering right */
	cgroup_delete_queue.emplace(path);

	/* delay deletion somewhat more so the daemon gets the chance
	   to read statistics */
	defer_cgroup_delete.Schedule(std::chrono::milliseconds{50});
}

void
Instance::OnDeferredCgroupDelete() noexcept
{
	/* delete the sorted set in reverse order */
	for (auto i = cgroup_delete_queue.rbegin();
	     i != cgroup_delete_queue.rend(); ++i)
		DestroyCgroup(root_cgroup, i->c_str());

	cgroup_delete_queue.clear();
}
