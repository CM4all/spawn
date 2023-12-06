// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Namespace.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <signal.h>

static UniqueSocketDescriptor
CreateBindLocalSocket(const char *path)
{
	UniqueSocketDescriptor s;
	if (!s.CreateNonBlock(AF_LOCAL, SOCK_SEQPACKET, 0))
		throw MakeErrno("Failed to create socket");

	s.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);

	{
		AllocatedSocketAddress address;
		address.SetLocal(path);
		if (!s.Bind(address))
			throw MakeErrno("Failed to bind");
	}

	if (!s.Listen(64))
		throw MakeErrno("Failed to listen");

	return s;
}

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload))
{
#ifdef HAVE_LIBSYSTEMD
	if (int n = sd_listen_fds(true); n > 0) {
		/* launched with systemd socket activation */
		for (int i = 0; i < n; ++i) {
			listeners.emplace_front(event_loop, *this);
			listeners.front().Listen(UniqueSocketDescriptor{SD_LISTEN_FDS_START + i});
		}
	} else {
#endif // HAVE_LIBSYSTEMD
		listeners.emplace_front(event_loop, *this);
		listeners.front().Listen(CreateBindLocalSocket("@cm4all-spawn"));
#ifdef HAVE_LIBSYSTEMD
	}
#endif // HAVE_LIBSYSTEMD

	shutdown_listener.Enable();
	sighup_event.Enable();
}

Instance::~Instance() noexcept = default;

void
Instance::OnExit() noexcept
{
	if (should_exit)
		return;

	should_exit = true;

	listeners.clear ();

	shutdown_listener.Disable();
	sighup_event.Disable();
}

void
Instance::OnReload(int) noexcept
{
}
