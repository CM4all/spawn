/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

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

static void
DestroyCgroup(const CgroupState &state, const char *relative_path)
{
	// TODO: collect statistics

	for (const auto &mount : state.mounts) {
		char buffer[4096];
		snprintf(buffer, sizeof(buffer), "/sys/fs/cgroup/%s%s",
			 mount.c_str(), relative_path);
		if (rmdir(buffer) < 0)
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

	// TODO: delay this call?
	DestroyCgroup(cgroup_state, path);
}
