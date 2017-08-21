/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "util/StringCompare.hxx"

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

void
Instance::OnSystemdAgentReleased(const char *path)
{
	const char *suffix = GetManagedSuffix(path);
	if (suffix == nullptr)
		return;

	printf("Cgroup released: %s\n", path);

	// TODO: implement
}
