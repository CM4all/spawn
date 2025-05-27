// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <sys/types.h>

class Namespace {
	UniqueFileDescriptor ipc_ns;
	UniqueFileDescriptor pid_ns;
	pid_t pid_init = 0;

public:
	~Namespace() noexcept;

	FileDescriptor MakeIpc();
	FileDescriptor MakePid();
};
