// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "system/SetupProcess.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_LIBSODIUM
#include "lua/sodium/Init.hxx"
#endif

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdio.h>
#include <stdlib.h>

static void
Run()
{
	SetupProcess();

	Instance instance;

#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif // HAVE_LIBSYSTEMD

	/* main loop */
	instance.Run();
}

int
main(int argc, char **argv)
try {
	(void)argc;
	(void)argv;

	/* force line buffering so Lua "print" statements are flushed
	   even if stdout is a pipe to systemd-journald */
	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IOLBF, 0);

	Run();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
