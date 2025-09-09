// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"

#include <map>
#include <string>

#include <sys/types.h>

class Namespace : public IntrusiveHashSetHook<IntrusiveHookMode::AUTO_UNLINK> {
	const std::string name;

	UniqueFileDescriptor ipc_ns;
	UniqueFileDescriptor pid_ns;
	std::map<std::string, UniqueFileDescriptor, std::less<>> user_namespaces;

	/**
	 * The pidfd of the PID namespace init process.
	 */
	PipeEvent pid_init;

	/**
	 * Inner class for lease pipe management.
	 */
	class Lease;

	/**
	 * List of all lease pipes.
	 */
	IntrusiveList<Lease> leases;

	/**
	 * Timer to be called after the last lease gets removed.  It
	 * discards this unused namespace.
	 */
	CoarseTimerEvent expire_timer;

public:
	Namespace(EventLoop &event_loop, std::string_view _name) noexcept;
	~Namespace() noexcept;

	std::string_view GetName() const noexcept {
		return name;
	}

	FileDescriptor MakeIpc();
	FileDescriptor MakePid();
	FileDescriptor MakeUser(std::string_view payload);
	UniqueFileDescriptor MakeLeasePipe();

private:
	int KillPidInit(int sig) noexcept;
	void OnPidInitExit(int status) noexcept;
	void OnPidfdReady(unsigned events) noexcept;
	void OnLeaseReleased(Lease &lease) noexcept;
	void OnExpireTimer() noexcept;
};
