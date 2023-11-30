// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LAccounting.hxx"
#include "CgroupAccounting.hxx"
#include "lua/Assert.hxx"
#include "lua/Resume.hxx"
#include "lua/io/XattrTable.hxx"
#include "io/FileAt.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"

using namespace Lua;

static void
Push(lua_State *L, CgroupCpuStat::Duration d)
{
	Push(L, std::chrono::duration_cast<std::chrono::duration<double>>(d).count());
}

static void
Push(lua_State *L, UniqueFileDescriptor &&cgroup_fd,
     const char *relative_path, const CgroupResourceUsage &usage)
{
	const ScopeCheckStack check_stack{L, 1};

	lua_newtable(L);

	SetField(L, RelativeStackIndex{-1}, "cgroup", relative_path);

	if (cgroup_fd.IsDefined()) {
		NewXattrTable(L, std::move(cgroup_fd));
		lua_setfield(L, -2, "cgroup_xattr");
	}

	if (usage.cpu.total.count() >= 0)
		SetField(L, RelativeStackIndex{-1}, "cpu_total", usage.cpu.total);

	if (usage.cpu.user.count() >= 0)
		SetField(L, RelativeStackIndex{-1}, "cpu_user", usage.cpu.user);

	if (usage.cpu.system.count() >= 0)
		SetField(L, RelativeStackIndex{-1}, "cpu_system", usage.cpu.system);

	if (usage.have_memory_peak)
		SetField(L, RelativeStackIndex{-1}, "memory_peak",
			 (lua_Integer)usage.memory_peak);
}

inline void
LuaAccounting::Thread::Start(const Lua::Value &_handler,
			     UniqueFileDescriptor &&cgroup_fd,
			     const char *relative_path,
			     const CgroupResourceUsage &usage) noexcept
{
	/* create a new thread for the handler coroutine */
	const auto L = runner.CreateThread(*this);

	_handler.Push(L);
	Push(L, std::move(cgroup_fd), relative_path, usage);
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
LuaAccounting::InvokeCgroupReleased(UniqueFileDescriptor cgroup_fd,
				    const char *relative_path,
				    const CgroupResourceUsage &usage)
{
	auto *thread = new Thread(GetState());
	threads.push_back(*thread);
	thread->Start(*handler, std::move(cgroup_fd), relative_path, usage);
}
