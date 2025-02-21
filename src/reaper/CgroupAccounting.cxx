// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupAccounting.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/PrintException.hxx"

using std::string_view_literals::operator""sv;

static size_t
ReadFile(FileDescriptor fd, std::span<std::byte> dest)
{
	ssize_t nbytes = fd.Read(dest);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	return nbytes;
}

static char *
ReadFileZ(FileDescriptor fd, char *buffer, size_t buffer_size)
{
	size_t length = ReadFile(fd, std::as_writable_bytes(std::span{buffer, buffer_size - 1}));
	buffer[length] = 0;
	return buffer;
}

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

static CgroupCpuStat
ReadCgroupCpuStat(FileDescriptor cgroup_fd)
{
	char buffer[4096];
	const char *data = ReadFileZ(OpenReadOnly(cgroup_fd, "cpu.stat"),
				     buffer, sizeof(buffer));

	CgroupCpuStat result;

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

	if (UniqueFileDescriptor fd; fd.OpenReadOnly(cgroup_fd, "memory.peak")) {
		char buffer[64];
		ssize_t nbytes = fd.Read(std::as_writable_bytes(std::span{buffer}));
		if (nbytes > 0 && static_cast<std::size_t>(nbytes) < sizeof(buffer)) {
			buffer[nbytes] = 0;

			char *endptr;
			result.memory_peak = strtoull(buffer, &endptr, 10);
			if (endptr > buffer)
				result.have_memory_peak = true;
		}
	}

	return result;
}
