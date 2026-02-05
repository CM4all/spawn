// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "LInit.hxx"
#include "LResolver.hxx"
#include "config.h"
#include "lua/Resume.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lua/net/ControlClient.hxx"
#include "lua/net/SocketAddress.hxx"

#ifdef HAVE_PG
#include "lua/pg/Init.hxx"
#endif

#ifdef HAVE_LIBSODIUM
#include "lua/sodium/Init.hxx"
#endif

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

Lua::State
LuaInit([[maybe_unused]] EventLoop &event_loop)
{
	Lua::State state{luaL_newstate()};
	auto *L = state.get();

	luaL_openlibs(state.get());
	Lua::InitResume(state.get());

#ifdef HAVE_LIBSODIUM
	Lua::InitSodium(state.get());
#endif

	Lua::InitSocketAddress(L);
	Lua::InitControlClient(L);
	RegisterLuaResolver(L);

	Lua::InitXattrTable(state.get());
	Lua::RegisterCgroupInfo(state.get());

#ifdef HAVE_PG
	Lua::InitPg(state.get(), event_loop);
#endif

	return state;
}
