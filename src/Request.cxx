/*
 * Copyright 2017-2018 Content Management AG
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

#include "Request.hxx"
#include "spawn/Protocol.hxx"
#include "util/StringView.hxx"

#include <stdexcept>

using namespace SpawnDaemon;

static std::string
CheckNonEmptyASCII(StringView payload)
{
	if (payload.empty())
		throw std::runtime_error("Empty string");

	for (char ch : payload)
		if ((signed char)ch < 0x20)
			throw std::runtime_error("Malformed string");

	return {payload.data, payload.size};
}

static std::string
CheckNonEmptyASCII(ConstBuffer<void> payload)
{
	return CheckNonEmptyASCII(StringView((const char *)payload.data,
					     payload.size));
}

void
SpawnRequest::Apply(RequestCommand command, ConstBuffer<void> payload)
{
	printf("Received cmd=%u size=%zu\n", unsigned(command), payload.size);

	switch (command) {
	case RequestCommand::NOP:
		break;

	case RequestCommand::NAME:
		if (!name.empty())
			throw std::runtime_error("Duplicate NAME");

		name = CheckNonEmptyASCII(payload);
		break;

	case RequestCommand::IPC_NAMESPACE:
		if (ipc_namespace)
			throw std::runtime_error("Duplicate IPC_NAMESPACE");

		if (!payload.empty())
			throw std::runtime_error("Malformed IPC_NAMESPACE");

		ipc_namespace = true;
		break;

	case RequestCommand::PID_NAMESPACE:
		if (pid_namespace)
			throw std::runtime_error("Duplicate PID_NAMESPACE");

		if (!payload.empty())
			throw std::runtime_error("Malformed PID_NAMESPACE");

		pid_namespace = true;
		break;
	}
}
