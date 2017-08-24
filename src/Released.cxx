/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

/**
 * These systemd scopes are allocated by our software which uses the
 * process spawner.  Their cgroups are managed by this daemon.
 */
static constexpr const char *managed_scopes[] = {
	"/system.slice/cm4all-beng-spawn.scope/",
	"/system.slice/cm4all-workshop-spawn.scope/",
	"/system.slice/cm4all-openssh.scope/",
};

static const char *
GetManagedSuffix(const char *path)
{
	for (const char *i : managed_scopes) {
		const char *suffix = StringAfterPrefix(path, i);
		if (suffix != nullptr)
			return suffix;
	}

	return nullptr;
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

static uint64_t
ReadCgroupNumber(const char *relative_path, const char *controller_name,
		 const char *filename)
{
	auto fd = OpenCgroupFile(relative_path, controller_name, filename);
	char data[64];
	ssize_t nbytes = fd.Read(data, sizeof(data) - 1);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	data[nbytes] = 0;

	char *endptr;
	auto value = strtoull(data, &endptr, 10);
	if (endptr == data)
		throw std::runtime_error("Failed to parse number");

	return value;
}

static void
CollectCgroupStats(const char *relative_path, const char *suffix)
{
	// TODO: blkio
	// TODO: multicast statistics

	char buffer[4096];
	size_t position = 0;

	try {
		position += sprintf(buffer + position, " cpuacct.usage=%" PRIu64,
				    ReadCgroupNumber(relative_path, "cpuacct", "usage"));
	} catch (...) {
		PrintException(std::current_exception());
	}

	try {
		position += sprintf(buffer + position, " cpuacct.usage_user=%" PRIu64,
				    ReadCgroupNumber(relative_path, "cpuacct", "usage_user"));
	} catch (...) {
		PrintException(std::current_exception());
	}

	try {
		position += sprintf(buffer + position, " cpuacct.usage_sys=%" PRIu64,
				    ReadCgroupNumber(relative_path, "cpuacct", "usage_sys"));
	} catch (...) {
		PrintException(std::current_exception());
	}

	try {
		position += sprintf(buffer + position,
				    " memory.max_usage_in_bytes=%" PRIu64,
				    ReadCgroupNumber(relative_path, "memory",
						     "max_usage_in_bytes"));
	} catch (...) {
		PrintException(std::current_exception());
	}

	if (position > 0)
		fprintf(stderr, "%s:%.*s\n", suffix, int(position), buffer);
}

static void
DestroyCgroup(const CgroupState &state, const char *relative_path)
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
Instance::OnSystemdAgentReleased(const char *path)
{
	const char *suffix = GetManagedSuffix(path);
	if (suffix == nullptr)
		return;

	printf("Cgroup released: %s\n", path);

	CollectCgroupStats(path, suffix);
	fflush(stdout);

	// TODO: delay this call?
	DestroyCgroup(cgroup_state, path);
}
