// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LInit.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/io/CgroupInfo.hxx"

#ifdef HAVE_PG
#include "lua/pg/Init.hxx"
#endif

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

Lua::State
LuaInit([[maybe_unused]] EventLoop &event_loop)
{
	Lua::State state{luaL_newstate()};

	luaL_openlibs(state.get());

	Lua::InitXattrTable(state.get());
	Lua::RegisterCgroupInfo(state.get());

#ifdef HAVE_PG
	Lua::InitPg(state.get(), event_loop);
#endif

	return state;
}
