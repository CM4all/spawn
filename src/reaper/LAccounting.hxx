// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/ReloadRunner.hxx"
#include "lua/State.hxx"
#include "lua/ValuePtr.hxx"
#include "util/IntrusiveList.hxx"

#include <chrono>

class UniqueFileDescriptor;
struct CgroupResourceUsage;

class LuaAccounting final {
	Lua::State state;

	Lua::ReloadRunner reload{state.get()};

	const Lua::ValuePtr handler;

	class Thread;

	IntrusiveList<Thread> threads;

public:
	LuaAccounting(Lua::State _state, Lua::ValuePtr _handler) noexcept;

	~LuaAccounting() noexcept;

	void Reload() noexcept {
		reload.Start();
	}

	void InvokeCgroupReleased(UniqueFileDescriptor cgroup_fd,
				  const char *relative_path,
				  std::chrono::system_clock::time_point btime,
				  const CgroupResourceUsage &usage);

private:
	lua_State *GetState() const noexcept {
		return handler->GetState();
	}
};
