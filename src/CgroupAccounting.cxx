// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupAccounting.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "spawn/CgroupState.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "util/PrintException.hxx"

using std::string_view_literals::operator""sv;

static UniqueFileDescriptor
OpenCgroupUnifiedFile(FileDescriptor v2_mount,
		      const char *relative_path, const char *filename)
{
	const auto path = FmtBuffer<4096>("{}/{}",
					  relative_path + 1, filename);

	return OpenReadOnly(v2_mount, path.c_str());
}

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
ReadCgroupCpuStat(FileDescriptor v2_mount, const char *relative_path)
{
	char buffer[4096];
	const char *data = ReadFileZ(OpenCgroupUnifiedFile(v2_mount,
							   relative_path,
							   "cpu.stat"),
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
ReadCgroupResourceUsage(const CgroupState &state,
			const char *relative_path) noexcept
{
	// TODO: blkio

	CgroupResourceUsage result;

	try {
		result.cpu = ReadCgroupCpuStat(state.group_fd,
					       relative_path);
	} catch (...) {
		PrintException(std::current_exception());
	}

	/* cgroup2 doesn't have something like
	   "memory.max_usage_in_bytes" */

	return result;
}
