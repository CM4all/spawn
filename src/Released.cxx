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
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"

#include <chrono>

#include <fcntl.h> // for AT_REMOVEDIR
#include <stdio.h>
#include <string.h>
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

static FileDescriptor
GetCgroupControllerMount(const CgroupState &state,
			 std::string_view controller) noexcept
{
	const auto c = state.controllers.find(controller);
	if (c == state.controllers.end())
		return FileDescriptor::Undefined();

	const std::string &mount_name = c->second;

	for (const auto &m : state.mounts)
		if (m.name == mount_name)
			return m.fd;

	return FileDescriptor::Undefined();
}

static UniqueFileDescriptor
OpenCgroupController(const CgroupState &state,
		     std::string_view controller,
		     const char *relative_path) noexcept
{
	assert(*relative_path == '/');
	assert(relative_path[1] != 0);

	const auto controller_mount =
		GetCgroupControllerMount(state, controller);
	if (!controller_mount.IsDefined())
		return {};

	return OpenPath(controller_mount, relative_path + 1);
}

static UniqueFileDescriptor
OpenCgroupUnifiedFile(FileDescriptor v2_mount,
		      const char *relative_path, const char *filename)
{
	char path[4096];
	snprintf(path, sizeof(path), "%s/%s",
		 relative_path + 1, filename);

	return OpenReadOnly(v2_mount, path);
}

static size_t
ReadFile(FileDescriptor fd, void *buffer, size_t buffer_size)
{
	ssize_t nbytes = fd.Read(buffer, buffer_size);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	return nbytes;
}

static char *
ReadFileZ(FileDescriptor fd, char *buffer, size_t buffer_size)
{
	size_t length = ReadFile(fd, buffer, buffer_size - 1);
	buffer[length] = 0;
	return buffer;
}

static uint64_t
ReadFileUint64(FileDescriptor fd)
{
	char buffer[64];
	const char *data = ReadFileZ(fd, buffer, sizeof(buffer));

	char *endptr;
	auto value = strtoull(data, &endptr, 10);
	if (endptr == data)
		throw std::runtime_error("Failed to parse number");

	return value;
}

static uint64_t
ReadFileUint64(FileDescriptor directory_fd, const char *filename)
{
	return ReadFileUint64(OpenReadOnly(directory_fd, filename));
}

static std::chrono::duration<double>
ReadFileNS(FileDescriptor fd)
{
	const auto value = ReadFileUint64(fd);
	return std::chrono::nanoseconds(value);
}

static std::chrono::duration<double>
ReadFileNS(FileDescriptor directory_fd, const char *filename)
{
	return ReadFileNS(OpenReadOnly(directory_fd, filename));
}

struct CpuStat {
	std::chrono::duration<double> total{}, user{}, system{};
};

static const char *
FindLine(const char *data, const char *name)
{
	const size_t name_length = strlen(name);
	const char *p = data;

	while (true) {
		const char *needle = strstr(p, name);
		if (needle == nullptr)
			break;

		if ((needle == data || needle[-1] == '\n') && needle[name_length] == ' ')
			return needle + name_length + 1;

		p = needle + 1;
	}

	return nullptr;
}

static CpuStat
ReadCgroupCpuStat(FileDescriptor v2_mount, const char *relative_path)
{
	char buffer[4096];
	const char *data = ReadFileZ(OpenCgroupUnifiedFile(v2_mount,
							   relative_path,
							   "cpu.stat"),
				     buffer, sizeof(buffer));

	CpuStat result;

	const char *p = FindLine(data, "usage_usec");
	if (p != nullptr)
		result.total = std::chrono::microseconds(strtoull(p, nullptr, 10));

	p = FindLine(data, "user_usec");
	if (p != nullptr)
		result.user = std::chrono::microseconds(strtoull(p, nullptr, 10));

	p = FindLine(data, "system_usec");
	if (p != nullptr)
		result.system = std::chrono::microseconds(strtoull(p, nullptr, 10));

	return result;
}

static void
CollectCgroupStats(const char *relative_path, const char *suffix,
		   const CgroupState &state)
{
	// TODO: blkio
	// TODO: multicast statistics

	char buffer[4096];
	size_t position = 0;

	bool have_cpu_stat = false;

	if (const auto v2_mount = state.GetUnifiedGroupMount();
	    v2_mount.IsDefined()) {
		try {
			const auto cpu_stat = ReadCgroupCpuStat(v2_mount,
								relative_path);
			position += sprintf(buffer + position, " cpu=%fs/%fs/%fs",
					    cpu_stat.total.count(),
					    cpu_stat.user.count(),
					    cpu_stat.system.count());
			have_cpu_stat = true;
		} catch (...) {
			PrintException(std::current_exception());
		}
	}

	if (auto cpuacct_fd = have_cpu_stat
	    ? UniqueFileDescriptor{}
	    : OpenCgroupController(state, "cpuacct"sv, relative_path);
	    cpuacct_fd.IsDefined()) {
		try {
			position += sprintf(buffer + position, " cpuacct.usage=%fs",
					    ReadFileNS(cpuacct_fd, "cpuacct.usage").count());
		} catch (...) {
			PrintException(std::current_exception());
		}

		try {
			position += sprintf(buffer + position, " cpuacct.usage_user=%fs",
					    ReadFileNS(cpuacct_fd, "cpuacct.usage_user").count());
		} catch (...) {
			PrintException(std::current_exception());
		}

		try {
			position += sprintf(buffer + position, " cpuacct.usage_sys=%fs",
					    ReadFileNS(cpuacct_fd, "cpuacct.usage_sys").count());
		} catch (...) {
			PrintException(std::current_exception());
		}
	}

	/* cgroup2 doesn't have something like
	   "memory.max_usage_in_bytes" */
	if (auto memory_fd = state.memory_v2
	    ? UniqueFileDescriptor{}
	    : OpenCgroupController(state, "memory"sv, relative_path);
	    memory_fd.IsDefined()) {
		try {
			static constexpr uint64_t MEGA = 1024 * 1024;

			position += sprintf(buffer + position,
					    " memory=%" PRIu64 "M",
					    (ReadFileUint64(memory_fd, "memory.max_usage_in_bytes") + MEGA / 2 - 1) / MEGA);
		} catch (...) {
			PrintException(std::current_exception());
		}
	}

	if (position > 0)
		fprintf(stderr, "%s:%.*s\n", suffix, int(position), buffer);
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

	CollectCgroupStats(path, suffix, cgroup_state);
	fflush(stdout);

	/* defer the deletion, because unpopulated children of this
	   cgroup may still exist; this deferral attempts to get the
	   ordering right */
	cgroup_delete_queue.emplace(path);
	defer_cgroup_delete.Schedule();
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
