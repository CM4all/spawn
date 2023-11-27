// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/Resume.hxx"
#include "lua/State.hxx"
#include "lua/ValuePtr.hxx"
#include "lua/CoRunner.hxx"
#include "util/IntrusiveList.hxx"

struct CgroupResourceUsage;

class LuaAccounting final {
	Lua::State state;

	const Lua::ValuePtr handler;

	class Thread final : public AutoUnlinkIntrusiveListHook, Lua::ResumeListener {
		/**
		 * The Lua thread which runs the handler coroutine.
		 */
		Lua::CoRunner runner;

	public:
		explicit Thread(lua_State *L) noexcept
			:runner(L) {}

		~Thread() noexcept {
			runner.Cancel();
		}

		void Start(const Lua::Value &handler,
			   const char *relative_path,
			   const CgroupResourceUsage &usage) noexcept;

		/* virtual methods from class ResumeListener */
		void OnLuaFinished(lua_State *L) noexcept override;
		void OnLuaError(lua_State *L,
				std::exception_ptr e) noexcept override;
	};

	IntrusiveList<Thread> threads;

public:
	explicit LuaAccounting(Lua::State &&_state,
			       Lua::ValuePtr &&_handler) noexcept
		:state(std::move(_state)),
		 handler(std::move(_handler)) {}

	~LuaAccounting() noexcept;

	void InvokeCgroupReleased(const char *relative_path,
				  const CgroupResourceUsage &usage);

private:
	lua_State *GetState() const noexcept {
		return handler->GetState();
	}
};
