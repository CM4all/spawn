// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Namespace.hxx"
#include "spawn/Init.hxx"
#include "system/linux/clone3.h"
#include "system/linux/PidFD.h"
#include "system/Error.hxx"
#include "io/linux/ProcPid.hxx"
#include "io/Pipe.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/WriteFile.hxx"
#include "util/StringSplit.hxx"

#include <fmt/core.h>

#include <cassert>
#include <cstdint>

#include <sched.h>
#include <signal.h>
#include <sys/pidfd.h>
#include <sys/wait.h> // for waitid()

using std::string_view_literals::operator""sv;

Namespace::Namespace(EventLoop &event_loop) noexcept
	:pid_init(event_loop, BIND_THIS_METHOD(OnPidfdReady))
{
}

Namespace::~Namespace() noexcept
{
	if (pid_init.IsDefined())
		KillPidInit(SIGTERM);
}

/**
 * Clone a child process with the specified flags, and invoke the
 * specified function with a /proc/PID file descriptor.
 */
static auto
WithPipeChild(uint_least64_t flags, std::invocable<FileDescriptor> auto f)
{
	auto [r, w] = CreatePipe();

	const struct clone_args ca{
		.flags = CLONE_CLEAR_SIGHAND|flags,
		.exit_signal = SIGCHLD,
	};

	const pid_t pid = clone3(&ca, sizeof(ca));
	if (pid < 0)
		throw MakeErrno("clone3() failed");

	if (pid == 0) {
		w.Close();

		std::byte buffer[1];
		(void)r.Read(buffer);
		_exit(0);
	}

	r.Close();

	return f(OpenProcPid(pid));
}

FileDescriptor
Namespace::MakeIpc()
{
	if (ipc_ns.IsDefined())
		return ipc_ns;

	WithPipeChild(CLONE_NEWIPC, [this](FileDescriptor proc_pid){
		if (!ipc_ns.OpenReadOnly(proc_pid, "ns/ipc"))
			throw MakeErrno("Failed to open /proc/PID/ns/ipc");
	});

	return ipc_ns;
}

FileDescriptor
Namespace::MakePid()
{
	if (pid_ns.IsDefined())
		return pid_ns;

	assert(!pid_init.IsDefined());

	const auto pid = UnshareForkSpawnInit();
	const int pidfd = my_pidfd_open(pid, PIDFD_NONBLOCK);
	if (pidfd < 0)
		throw MakeErrno("pidfd_open() failed");

	pid_init.Open(FileDescriptor{pidfd});
	pid_init.ScheduleRead();

	try {
		const auto proc_pid = OpenProcPid(pid);

		if (!pid_ns.OpenReadOnly(proc_pid, "ns/pid"))
			throw MakeErrno("Failed to open /proc/PID/ns/pid");

		return pid_ns;
	} catch (...) {
		KillPidInit(SIGTERM);
		pid_init.Close();
		throw;
	}
}

FileDescriptor
Namespace::MakeUser(std::string_view payload)
{
	if (auto i = user_namespaces.find(payload); i != user_namespaces.end())
		return i->second;

	return WithPipeChild(CLONE_NEWUSER, [this, payload](FileDescriptor proc_pid) -> FileDescriptor {
		UniqueFileDescriptor user_ns;
		if (!user_ns.OpenReadOnly(proc_pid, "ns/user"))
			throw MakeErrno("Failed to open /proc/PID/ns/user");

		/* split payload into uid_map and gid_map */
		const auto [uid_map, gid_map] = Split(payload, '\0');

		if (!uid_map.empty()) {
			const auto result = TryWriteExistingFile(proc_pid, "uid_map", uid_map);
			if (result == WriteFileResult::ERROR)
				throw MakeErrno("Failed to write uid_map");
		}

		if (!gid_map.empty()) {
			const auto result = TryWriteExistingFile(proc_pid, "gid_map", gid_map);
			if (result == WriteFileResult::ERROR)
				throw MakeErrno("Failed to write gid_map");
		}

		auto [it, inserted] = user_namespaces.emplace(std::string{payload}, std::move(user_ns));
		assert(inserted);
		return it->second;
	});
}

inline int
Namespace::KillPidInit(int sig) noexcept
{
	assert(pid_init.IsDefined());

	return my_pidfd_send_signal(pid_init.GetFileDescriptor().Get(),
				    sig, nullptr, 0);
}

inline void
Namespace::OnPidInitExit([[maybe_unused]] int status) noexcept
{
	assert(pid_init.IsDefined());

	pid_init.Close();
	pid_ns.Close();
}

inline void
Namespace::OnPidfdReady([[maybe_unused]] unsigned events) noexcept
{
	assert(pid_init.IsDefined());

	siginfo_t info;
	info.si_pid = 0;

	if (waitid((idtype_t)P_PIDFD, pid_init.GetFileDescriptor().Get(),
		   &info, WEXITED|WNOHANG) < 0) {
		const int e = errno;
		/* errno==ECHILD can happen if the child has exited
		   while ZombieReaper was already running (because
		   many child processes have exited at the same time)
		   - pretend the child has exited */
		if (e != ECHILD)
			fmt::print(stderr, "waitid() failed: {}\n"sv, strerror(e));
		OnPidInitExit(-e);
		return;
	}

	if (info.si_pid == 0)
		return;

	int status;

	switch (info.si_code) {
	case CLD_EXITED:
		status = W_EXITCODE(info.si_status, 0);
		break;

	case CLD_KILLED:
		status = W_STOPCODE(info.si_status);

		fmt::print(stderr, "PID namespace init died from signal {}\n"sv,
			   info.si_status);
		break;

	case CLD_DUMPED:
		status = W_STOPCODE(info.si_status) | WCOREFLAG;

		fmt::print(stderr, "PID namespace init died from signal {} (core dumped)\n"sv,
			   info.si_status);
		break;

	case CLD_STOPPED:
	case CLD_TRAPPED:
	case CLD_CONTINUED:
		return;
	}

	OnPidInitExit(status);
}
