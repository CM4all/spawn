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

	void InvokeCgroupReleased(const CgroupResourceUsage &usage);

private:
	lua_State *GetState() const noexcept {
		return handler->GetState();
	}
};
