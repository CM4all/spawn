// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <map>
#include <string>

#include <sys/types.h>

class Namespace {
	UniqueFileDescriptor ipc_ns;
	UniqueFileDescriptor pid_ns;
	std::map<std::string, UniqueFileDescriptor, std::less<>> user_namespaces;

	/**
	 * The pidfd of the PID namespace init process.
	 */
	PipeEvent pid_init;

public:
	explicit Namespace(EventLoop &event_loop) noexcept;
	~Namespace() noexcept;

	FileDescriptor MakeIpc();
	FileDescriptor MakePid();
	FileDescriptor MakeUser(std::string_view payload);

private:
	int KillPidInit(int sig) noexcept;
	void OnPidInitExit(int status) noexcept;
	void OnPidfdReady(unsigned events) noexcept;
};
