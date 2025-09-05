// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <map>
#include <string>
#include <sys/types.h>

class Namespace {
	UniqueFileDescriptor ipc_ns;
	UniqueFileDescriptor pid_ns;
	std::map<std::string, UniqueFileDescriptor, std::less<>> user_namespaces;
	pid_t pid_init = 0;

public:
	~Namespace() noexcept;

	FileDescriptor MakeIpc();
	FileDescriptor MakePid();
	FileDescriptor MakeUser(std::string_view payload);
};
