// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ReaperInstance.hxx"
#include "system/SetupProcess.hxx"
#include "util/PrintException.hxx"

#include <systemd/sd-daemon.h>

#include <stdlib.h>

static void
Run()
{
	SetupProcess();

	Instance instance;

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	/* main loop */
	instance.Run();
}

int
main(int argc, char **argv)
try {
	(void)argc;
	(void)argv;

	Run();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
