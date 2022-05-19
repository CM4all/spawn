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

#include "Instance.hxx"
#include "Namespace.hxx"
#include "Scopes.hxx"
#include "UnifiedWatch.hxx"
#include "LAccounting.hxx"
#include "LInit.hxx"
#include "lua/RunFile.hxx"
#include "spawn/Systemd.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"

#include <signal.h>
#include <unistd.h>

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

static auto
CreateUnifiedCgroupWatch(EventLoop &event_loop,
			 const CgroupState &cgroup_state,
			 auto callback)
{
	if (!cgroup_state.IsEnabled())
		throw std::runtime_error("systemd cgroups are not available");

	const auto unified_mount = cgroup_state.GetUnifiedMountPath();
	if (unified_mount.empty())
		throw std::runtime_error("systemd unified cgroup is not available");

	auto watch = std::make_unique<UnifiedCgroupWatch>(event_loop,
							  unified_mount.c_str(),
							  callback);
	for (auto i = managed_scopes; *i != nullptr; ++i) {
		const char *relative_path = *i;
		if (*relative_path == '/')
			++relative_path;

		watch->AddCgroup(relative_path);
	}

	return watch;
}

static Lua::ValuePtr
GetGlobalFunction(lua_State *L, const char *name)
{
	lua_getglobal(L, name);
	AtScopeExit(L) { lua_pop(L, 1); };

	if (lua_isnil(L, -1))
		throw FormatRuntimeError("Function '%s' not found", name);

	if (!lua_isfunction(L, -1))
		throw FormatRuntimeError("'%s' is not a function", name);

	return std::make_shared<Lua::Value>(L, Lua::RelativeStackIndex{-1});
}

static std::unique_ptr<LuaAccounting>
LoadLuaAccounting(const char *path)
{
	auto state = LuaInit();
	Lua::RunFile(state.get(), path);

	auto handler = GetGlobalFunction(state.get(), "cgroup_released");

	return std::make_unique<LuaAccounting>(std::move(state),
					       std::move(handler));
}

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 listener(event_loop, *this),
	 /* kludge: opening "/." so CgroupState contains file
	    descriptors to the root cgroup */
	 cgroup_state(CgroupState::FromProcess(0, "/.")),
	 unified_cgroup_watch(CreateUnifiedCgroupWatch(event_loop, cgroup_state,
						       BIND_THIS_METHOD(OnSystemdAgentReleased))),
	 lua_accounting(LoadLuaAccounting("/etc/cm4all/spawn/accounting.lua")),
	 defer_cgroup_delete(event_loop,
			     BIND_THIS_METHOD(OnDeferredCgroupDelete))
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

	lua_accounting.reset();

	unified_cgroup_watch.reset();
}

void
Instance::OnReload(int) noexcept
{
}
