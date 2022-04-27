/*
 * Copyright 2017-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TreeWatch.hxx"
#include "event/Loop.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"

#include <stdlib.h>

class MyTreeWatch final : public TreeWatch {
public:
	MyTreeWatch(EventLoop &event_loop, const char *base_path)
		:TreeWatch(event_loop, base_path) {}


protected:
	void OnDirectoryCreated(const std::string &relative_path,
				FileDescriptor) noexcept override {
		printf("+ %s\n", relative_path.c_str());
	}

	void OnDirectoryDeleted(const std::string &relative_path) noexcept override {
		printf("- %s\n", relative_path.c_str());
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

	event_loop.Dispatch();

	return EXIT_SUCCESS;
} catch (const Usage &) {
	fprintf(stderr, "Usage: %s PATH REL1...\n", argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
