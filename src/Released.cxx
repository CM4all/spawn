/*
 * Copyright 2017-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Instance.hxx"
#include "Scopes.hxx"
#include "CgroupAccounting.hxx"
#include "LAccounting.hxx"
#include "util/StringCompare.hxx"

#include <fmt/format.h>

#include <fcntl.h> // for AT_REMOVEDIR
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

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
		   const CgroupResourceUsage &u)
{
	char buffer[4096];
	size_t position = 0;

	if (u.cpu.user.count() >= 0 || u.cpu.system.count() >= 0) {
		const auto user = std::max(u.cpu.user.count(), 0.);
		const auto system = std::max(u.cpu.user.count(), 0.);
		const auto total = u.cpu.total.count() >= 0
			? u.cpu.total.count()
			: user + system;

		position += sprintf(buffer + position, " cpu=%fs/%fs/%fs",
				    total, user, system);
	} else if (u.cpu.total.count() >= 0) {
		position += sprintf(buffer + position, " cpu=%fs",
				    u.cpu.total.count());
	}

	if (u.have_memory_max_usage) {
		static constexpr uint64_t MEGA = 1024 * 1024;

		position += sprintf(buffer + position,
				    " memory=%" PRIu64 "M",
				    (u.memory_max_usage + MEGA / 2 - 1) / MEGA);
	}

	if (position > 0)
		fmt::print(stderr, "{}:{}\n", suffix,
			   std::string_view{buffer, position});
}

static void
DestroyCgroup(const CgroupState &state, const char *relative_path) noexcept
{
	assert(*relative_path == '/');
	assert(relative_path[1] != 0);

	for (const auto &mount : state.mounts) {
		if (unlinkat(mount.fd.Get(), relative_path + 1,
			     AT_REMOVEDIR) < 0 &&
		    errno != ENOENT)
			fprintf(stderr, "Failed to delete '%s': %s\n",
				relative_path, strerror(errno));
	}
}

void
Instance::OnSystemdAgentReleased(const char *path) noexcept
{
	const char *suffix = GetManagedSuffix(path);
	if (suffix == nullptr)
		return;

	// TODO read resource usage right before the cgroup actually gets deleted
	const auto u = ReadCgroupResourceUsage(cgroup_state, path);

	CollectCgroupStats(suffix, u);

	if (lua_accounting)
		lua_accounting->InvokeCgroupReleased(u);

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
		DestroyCgroup(cgroup_state, i->c_str());

	cgroup_delete_queue.clear();
}
