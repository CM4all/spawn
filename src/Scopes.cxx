// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Scopes.hxx"

/**
 * These systemd scopes are allocated by our software which uses the
 * process spawner.  Their cgroups are managed by this daemon.
 */
const char *const managed_scopes[] = {
	"/system.slice/system-cm4all.slice/bp-spawn.scope/",
	"/system.slice/system-cm4all.slice/workshop-spawn.scope/",
	"/system.slice/system-cm4all.slice/openssh.scope/",

	nullptr,
};
