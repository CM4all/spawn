/*
 * Copyright 2017-2018 Content Management AG
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
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"

#include <chrono>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

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

static UniqueFileDescriptor
OpenCgroupUnifiedFile(const char *relative_path, const char *filename)
{
	char path[4096];
	snprintf(path, sizeof(path), "/sys/fs/cgroup/unified%s/%s",
		 relative_path, filename);

	UniqueFileDescriptor fd;
	if (!fd.OpenReadOnly(path))
		throw FormatErrno("Failed to open %s", path);
	return fd;
}

static UniqueFileDescriptor
OpenCgroupFile(const char *relative_path, const char *controller_name,
	       const char *filename)
{
	char path[4096];
	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s%s/%s.%s",
		 controller_name, relative_path,
		 controller_name, filename);

	UniqueFileDescriptor fd;
	if (!fd.OpenReadOnly(path))
		throw FormatErrno("Failed to open %s", path);
	return fd;
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

static std::chrono::duration<double>
ReadFileNS(FileDescriptor fd)
{
	const auto value = ReadFileUint64(fd);
	return std::chrono::nanoseconds(value);
}

static uint64_t
ReadCgroupNumber(const char *relative_path, const char *controller_name,
		 const char *filename)
{
	return ReadFileUint64(OpenCgroupFile(relative_path,
					     controller_name, filename));
}

static std::chrono::duration<double>
ReadCgroupNS(const char *relative_path, const char *controller_name,
	     const char *filename)
{
	return ReadFileNS(OpenCgroupFile(relative_path,
					 controller_name, filename));
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
ReadCgroupCpuStat(const char *relative_path)
{
	char buffer[4096];
	const char *data = ReadFileZ(OpenCgroupUnifiedFile(relative_path,
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
		   bool have_unified)
{
	// TODO: blkio
	// TODO: multicast statistics

	char buffer[4096];
	size_t position = 0;

	bool have_cpu_stat = false;

	if (have_unified) {
		try {
			const auto cpu_stat = ReadCgroupCpuStat(relative_path);
			position += sprintf(buffer + position, " cpu=%fs/%fs/%fs",
					    cpu_stat.total.count(),
					    cpu_stat.user.count(),
					    cpu_stat.system.count());
			have_cpu_stat = true;
		} catch (...) {
			PrintException(std::current_exception());
		}
	}

	if (!have_cpu_stat) {
		try {
			position += sprintf(buffer + position, " cpuacct.usage=%fs",
					    ReadCgroupNS(relative_path, "cpuacct", "usage").count());
		} catch (...) {
			PrintException(std::current_exception());
		}

		try {
			position += sprintf(buffer + position, " cpuacct.usage_user=%fs",
					    ReadCgroupNS(relative_path, "cpuacct", "usage_user").count());
		} catch (...) {
			PrintException(std::current_exception());
		}

		try {
			position += sprintf(buffer + position, " cpuacct.usage_sys=%fs",
					    ReadCgroupNS(relative_path, "cpuacct", "usage_sys").count());
		} catch (...) {
			PrintException(std::current_exception());
		}
	}

	try {
		static constexpr uint64_t MEGA = 1024 * 1024;

		position += sprintf(buffer + position,
				    " memory=%" PRIu64 "M",
				    (ReadCgroupNumber(relative_path, "memory",
						      "max_usage_in_bytes") + MEGA / 2 - 1) / MEGA);
	} catch (...) {
		PrintException(std::current_exception());
	}

	if (position > 0)
		fprintf(stderr, "%s:%.*s\n", suffix, int(position), buffer);
}

static void
DestroyCgroup(const CgroupState &state, const char *relative_path) noexcept
{
	for (const auto &mount : state.mounts) {
		char buffer[4096];
		snprintf(buffer, sizeof(buffer), "/sys/fs/cgroup/%s%s",
			 mount.c_str(), relative_path);
		if (rmdir(buffer) < 0 && errno != ENOENT)
			fprintf(stderr, "Failed to delete '%s': %s\n",
				buffer, strerror(errno));
	}
}

void
Instance::OnSystemdAgentReleased(const char *path) noexcept
{
	const char *suffix = GetManagedSuffix(path);
	if (suffix == nullptr)
		return;

	CollectCgroupStats(path, suffix, !!unified_cgroup_watch);
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
