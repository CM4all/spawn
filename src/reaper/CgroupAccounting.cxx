// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupAccounting.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"
#include "util/StringStrip.hxx"

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
ReadFileZ(FileDescriptor fd, std::span<char> dest)
{
	size_t length = ReadFile(fd, std::as_writable_bytes(dest.first(dest.size() - 1)));
	dest[length] = 0;
	return dest.data();
}

static std::string_view
FindLine(const char *data, const char *name)
{
	const size_t name_length = strlen(name);
	const char *p = data;

	while (true) {
		const char *needle = strstr(p, name);
		if (needle == nullptr)
			break;

		if ((needle == data || needle[-1] == '\n') && needle[name_length] == ' ') {
			const char *value = needle + name_length + 1;
			const char *end = strchr(value, '\n');
			if (end == nullptr)
				return value;
			else
				return {value, end};
		}

		p = needle + 1;
	}

	return {};
}

static CgroupCpuStat
ReadCgroupCpuStat(FileDescriptor cgroup_fd)
{
	char buffer[4096];
	const char *data = ReadFileZ(OpenReadOnly(cgroup_fd, "cpu.stat"),
				     buffer);

	CgroupCpuStat result;

	if (const auto line = FindLine(data, "usage_usec"); line.data() != nullptr)
		if (auto value = ParseInteger<uint_least64_t>(line))
			result.total = std::chrono::microseconds(*value);

	if (const auto line = FindLine(data, "user_usec"); line.data() != nullptr)
		if (auto value = ParseInteger<uint_least64_t>(line))
			result.user = std::chrono::microseconds(*value);

	if (const auto line = FindLine(data, "system_usec"); line.data() != nullptr)
		if (auto value = ParseInteger<uint_least64_t>(line))
			result.system = std::chrono::microseconds(*value);

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
			const std::string_view s{buffer, static_cast<std::size_t>(nbytes)};

			if (auto value = ParseInteger<uint_least64_t>(StripRight(s))) {
				result.memory_peak = *value;
				result.have_memory_peak = true;
			}
		}
	}

	return result;
}
