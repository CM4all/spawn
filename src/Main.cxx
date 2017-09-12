/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "system/SetupProcess.hxx"
#include "system/ProcessName.hxx"
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
	instance.Dispatch();
}

int
main(int argc, char **argv)
try {
	InitProcessName(argc, argv);

	Run();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
