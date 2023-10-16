// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ReaperInstance.hxx"
#include "Namespace.hxx"
#include "Scopes.hxx"
#include "UnifiedWatch.hxx"
#include "LAccounting.hxx"
#include "LInit.hxx"
#include "lua/RunFile.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"

#include <signal.h>

static auto
CreateUnifiedCgroupWatch(EventLoop &event_loop,
			 const CgroupState &cgroup_state,
			 auto callback)
{
	if (!cgroup_state.IsEnabled())
		throw std::runtime_error("systemd cgroups are not available");

	assert(cgroup_state.group_fd.IsDefined());

	auto watch = std::make_unique<UnifiedCgroupWatch>(event_loop,
							  cgroup_state.group_fd,
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
		throw FmtRuntimeError("Function '{}' not found", name);

	if (!lua_isfunction(L, -1))
		throw FmtRuntimeError("'{}' is not a function", name);

	return std::make_shared<Lua::Value>(L, Lua::RelativeStackIndex{-1});
}

static std::unique_ptr<LuaAccounting>
LoadLuaAccounting(EventLoop &event_loop, const char *path)
{
	auto state = LuaInit(event_loop);
	Lua::RunFile(state.get(), path);

	auto handler = GetGlobalFunction(state.get(), "cgroup_released");

	return std::make_unique<LuaAccounting>(std::move(state),
					       std::move(handler));
}

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 /* kludge: opening "/." so CgroupState contains file
	    descriptors to the root cgroup */
	 cgroup_state(CgroupState::FromProcess(0, "/.")),
	 unified_cgroup_watch(CreateUnifiedCgroupWatch(event_loop, cgroup_state,
						       BIND_THIS_METHOD(OnSystemdAgentReleased))),
	 lua_accounting(LoadLuaAccounting(event_loop,
					  "/etc/cm4all/spawn/accounting.lua")),
	 defer_cgroup_delete(event_loop,
			     BIND_THIS_METHOD(OnDeferredCgroupDelete))
{
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

	shutdown_listener.Disable();
	sighup_event.Disable();

	lua_accounting.reset();

	unified_cgroup_watch.reset();
}

void
Instance::OnReload(int) noexcept
{
}