// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LInit.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lua/pg/Init.hxx"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

Lua::State
LuaInit(EventLoop &event_loop)
{
	Lua::State state{luaL_newstate()};

	luaL_openlibs(state.get());

	Lua::InitXattrTable(state.get());
	Lua::RegisterCgroupInfo(state.get());
	Lua::InitPg(state.get(), event_loop);

	return state;
}
