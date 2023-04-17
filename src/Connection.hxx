// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/UdpListener.hxx"
#include "event/net/UdpHandler.hxx"
#include "util/IntrusiveList.hxx"

#include <string_view>

#include <sys/socket.h>

class Instance;
class UniqueSocketDescriptor;
struct SpawnRequest;

class SpawnConnection final
	: public AutoUnlinkIntrusiveListHook,
	UdpHandler {

	Instance &instance;

	UdpListener listener;

public:
	SpawnConnection(Instance &_instance,
			UniqueSocketDescriptor &&_fd, SocketAddress address);

private:
	void SendError(std::string_view msg);

	void OnMakeNamespaces(SpawnRequest &&request);
	void OnRequest(SpawnRequest &&request);

	/* virtual methods from class UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;
};
