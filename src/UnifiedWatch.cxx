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

#include "UnifiedWatch.hxx"
#include "event/SocketEvent.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/BindMethod.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"

#include <fcntl.h> // for AT_FDCWD
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool
IsPopulated(FileDescriptor fd) noexcept
{
	char buffer[4096];
	ssize_t nbytes = pread(fd.Get(), buffer, sizeof(buffer) - 1, 0);
	if (nbytes <= 0)
		return false;

	buffer[nbytes] = 0;
	return strstr(buffer, "populated 0") == nullptr;
}

class UnifiedCgroupWatch::Group {
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

UnifiedCgroupWatch::Group::Group(UnifiedCgroupWatch &_parent,
				 const std::string &_relative_path,
				 UniqueFileDescriptor &&_fd) noexcept
	:parent(_parent),
	 relative_path(_relative_path),
	 fd(std::move(_fd)),
	 event(parent.GetEventLoop(), BIND_THIS_METHOD(EventCallback),
	       SocketDescriptor::FromFileDescriptor(fd))
{
	event.ScheduleImplicit();
}

void
UnifiedCgroupWatch::Group::EventCallback(unsigned) noexcept
{
	if (!IsPopulated(fd))
		parent.OnGroupEmpty(*this);
}

UnifiedCgroupWatch::UnifiedCgroupWatch(EventLoop &event_loop,
				       const char *cgroup2_mount,
				       Callback _callback)
	:TreeWatch(event_loop, FileDescriptor{AT_FDCWD}, cgroup2_mount),
	 callback(_callback)
{
}

UnifiedCgroupWatch::~UnifiedCgroupWatch() noexcept = default;

void
UnifiedCgroupWatch::AddCgroup(const char *relative_path)
{
	assert(!in_add);
	in_add = true;
	AtScopeExit(this) { in_add = false; };

	TreeWatch::Add(relative_path);
}

void
UnifiedCgroupWatch::OnGroupEmpty(Group &group) noexcept
{
	callback(("/" + group.GetRelativePath()).c_str());

	auto i = groups.find(group.GetRelativePath());
	assert(i != groups.end());
	assert(&i->second == &group);
	groups.erase(i);
}

void
UnifiedCgroupWatch::OnDirectoryCreated(const std::string &relative_path,
				       FileDescriptor directory_fd) noexcept
{
	try {
		auto fd = OpenReadOnly(directory_fd, "cgroup.events");
		if (!in_add)
			/* if this new cgroup was just created, call
			   IsPopulated() to discard the initial event;
			   we don't want to auto-delete if it it's
			   empty, because we already know it's empty;
			   delete empty cgroups immediately only
			   during the initial scan, i.e. from inside
			   AddCgroup() */
			IsPopulated(fd);

		groups.emplace(std::piecewise_construct,
			       std::forward_as_tuple(relative_path),
			       std::forward_as_tuple(*this,
						     relative_path,
						     std::move(fd)));
	} catch (...) {
		PrintException(std::current_exception());
	}
}

void
UnifiedCgroupWatch::OnDirectoryDeleted(const std::string &relative_path) noexcept
{
	auto i = groups.find(relative_path);
	if (i == groups.end())
		return;

	groups.erase(i);
}
