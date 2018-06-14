/*
 * Copyright 2017-2018 Content Management AG
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

#include "TreeWatch.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "event/SocketEvent.hxx"
#include "util/BindMethod.hxx"

#include <map>
#include <string>

/**
 * Watch events in the "unified" (v2) cgroup hierarchy.
 */
class UnifiedCgroupWatch final : TreeWatch {
	typedef BoundMethod<void(const char *relative_path)> Callback;
	const Callback callback;

	class Group {
		UnifiedCgroupWatch &parent;

		const std::string relative_path;

		UniqueFileDescriptor fd;

		SocketEvent event;

	public:
		Group(UnifiedCgroupWatch &_parent,
		      const std::string &_relative_path,
		      UniqueFileDescriptor &&_fd) noexcept;

		const std::string &GetRelativePath() noexcept {
			return relative_path;
		}

	private:
		void EventCallback(unsigned events) noexcept;
	};

	std::map<std::string, Group> groups;

	bool in_add = false;

public:
	UnifiedCgroupWatch(EventLoop &event_loop, Callback _callback);

	void AddCgroup(const char *relative_path);

private:
	void OnGroupEmpty(Group &group) noexcept;

protected:
	void OnDirectoryCreated(const std::string &relative_path,
				FileDescriptor directory_fd) noexcept override;
	void OnDirectoryDeleted(const std::string &relative_path) noexcept override;
};

bool
HasUnifiedCgroups() noexcept;
