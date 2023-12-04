// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Namespace.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

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
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 listener(event_loop, *this)
{
	listener.Listen(CreateBindLocalSocket("@cm4all-spawn"));

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

	listener.RemoveEvent();
	listener.CloseAllConnections();

	shutdown_listener.Disable();
	sighup_event.Disable();
}

void
Instance::OnReload(int) noexcept
{
}
