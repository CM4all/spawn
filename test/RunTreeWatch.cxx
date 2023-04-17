// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "TreeWatch.hxx"
#include "event/Loop.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"

#include <fmt/format.h>

#include <fcntl.h> // for AT_FDCWD
#include <stdlib.h>

class MyTreeWatch final : public TreeWatch {
public:
	MyTreeWatch(EventLoop &event_loop, const char *base_path)
		:TreeWatch(event_loop, FileDescriptor{AT_FDCWD}, base_path) {}

protected:
	void OnDirectoryCreated(const std::string &relative_path,
				FileDescriptor) noexcept override {
		fmt::print("+ {}\n", relative_path);
	}

	void OnDirectoryDeleted(const std::string &relative_path) noexcept override {
		fmt::print("- {}\n", relative_path);
	}
};

struct Usage {};

int
main(int argc, char **argv)
try {
	ConstBuffer<const char *> args(argv + 1, argc - 1);

	if (args.size < 2)
		throw Usage();

	const char *base_path = args.shift();

	EventLoop event_loop;
	MyTreeWatch tw(event_loop, base_path);

	for (const char *relative_path : args)
		tw.Add(relative_path);

	event_loop.Run();

	return EXIT_SUCCESS;
} catch (const Usage &) {
	fmt::print(stderr, "Usage: {} PATH REL1...\n", argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
