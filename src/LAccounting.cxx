// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LAccounting.hxx"
#include "CgroupAccounting.hxx"
#include "lua/Assert.hxx"
#include "lua/Resume.hxx"
#include "lua/io/XattrTable.hxx"
#include "spawn/CgroupState.hxx"
#include "io/Beneath.hxx"
#include "io/FileAt.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"

using namespace Lua;

static void
Push(lua_State *L, const CgroupState &cgroup_state,
     const char *relative_path, const CgroupResourceUsage &usage)
{
	const ScopeCheckStack check_stack{L, 1};

	lua_newtable(L);

	SetField(L, RelativeStackIndex{-1}, "cgroup", relative_path);

	try {
		auto fd = OpenReadOnlyBeneath({cgroup_state.group_fd, relative_path + 1});
		NewXattrTable(L, std::move(fd));
		lua_setfield(L, -2, "cgroup_xattr");
	} catch (...) {
	}

	// TODO add more fields

	if (usage.have_memory_max_usage)
		SetField(L, RelativeStackIndex{-1}, "memory_max_usage",
			 (lua_Integer)usage.memory_max_usage);
}

void
LuaAccounting::Thread::Start(const Lua::Value &_handler,
			     const CgroupState &cgroup_state,
			     const char *relative_path,
			     const CgroupResourceUsage &usage) noexcept
{
	/* create a new thread for the handler coroutine */
	const auto L = runner.CreateThread(*this);

	_handler.Push(L);
	Push(L, cgroup_state, relative_path, usage);
	Resume(L, 1);
}

void
LuaAccounting::Thread::OnLuaFinished(lua_State *) noexcept
{
	delete this;
}

void
LuaAccounting::Thread::OnLuaError(lua_State *,
				  std::exception_ptr e) noexcept
{
	// TODO log more metadata?
	PrintException(e);
	delete this;
}

LuaAccounting::~LuaAccounting() noexcept
{
	threads.clear_and_dispose(DeleteDisposer{});
}

void
LuaAccounting::InvokeCgroupReleased(const CgroupState &cgroup_state,
				    const char *relative_path,
				    const CgroupResourceUsage &usage)
{
	auto *thread = new Thread(GetState());
	threads.push_back(*thread);
	thread->Start(*handler, cgroup_state, relative_path, usage);
}
