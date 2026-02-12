// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "UnifiedWatch.hxx"
#include "event/PipeEvent.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/BindMethod.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"

#include <fcntl.h> // for AT_FDCWD
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using std::string_view_literals::operator""sv;

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

	/**
	 * Polls for events on the "cgroup.events" file.
	 */
	PipeEvent event;

public:
	Group(UnifiedCgroupWatch &_parent,
	      std::string_view _relative_path,
	      UniqueFileDescriptor &&_fd) noexcept;

	~Group() noexcept {
		event.Close();
	}

	const std::string &GetRelativePath() noexcept {
		return relative_path;
	}

private:
	void EventCallback(unsigned events) noexcept;
};

inline
UnifiedCgroupWatch::Group::Group(UnifiedCgroupWatch &_parent,
				 std::string_view _relative_path,
				 UniqueFileDescriptor &&_fd) noexcept
	:parent(_parent),
	 relative_path(_relative_path),
	 event(parent.GetEventLoop(), BIND_THIS_METHOD(EventCallback),
	       _fd.Release())
{
	event.Schedule(event.EXCEPTIONAL);
}

void
UnifiedCgroupWatch::Group::EventCallback(unsigned) noexcept
{
	if (!IsPopulated(event.GetFileDescriptor()))
		parent.OnGroupEmpty(*this);
}

UnifiedCgroupWatch::UnifiedCgroupWatch(EventLoop &event_loop,
				       FileDescriptor cgroup2_mount,
				       Callback _callback)
	:TreeWatch(event_loop, cgroup2_mount, "."),
	 callback(_callback)
{
}

UnifiedCgroupWatch::~UnifiedCgroupWatch() noexcept = default;

void
UnifiedCgroupWatch::AddCgroup(std::string_view relative_path)
{
	assert(!in_add);
	in_add = true;
	AtScopeExit(this) { in_add = false; };

	TreeWatch::Add(relative_path);
}

void
UnifiedCgroupWatch::ReAddCgroup(std::string_view relative_path) noexcept
{
	if (const auto fd = TreeWatch::Find(relative_path); fd.IsDefined()) {
		try {
			InsertGroup(relative_path, fd, false);
		} catch (...) {
			PrintException(std::current_exception());
		}
	}
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
UnifiedCgroupWatch::InsertGroup(const std::string_view relative_path,
				FileDescriptor directory_fd,
				bool discard)
{
	auto fd = OpenReadOnly({directory_fd, "cgroup.events"});
	if (discard)
		/* discard the initial event by reading from the
		   "cgroup.events" file */
		IsPopulated(fd);

	groups.emplace(std::piecewise_construct,
		       std::forward_as_tuple(relative_path),
		       std::forward_as_tuple(*this,
					     relative_path,
					     std::move(fd)));
}

bool
UnifiedCgroupWatch::ShouldSkipName(std::string_view name) const noexcept
{
	/**
	 * List of well-known cgroup file names to be skipped by class
	 * #TreeWatch (don't bother to open() them while looking for
	 * new subdirectories).
	 */
	static constexpr std::string_view skip_names[] = {
		// must be sorted:

		"cgroup.controllers"sv,
		"cgroup.events"sv,
		"cgroup.freeze"sv,
		"cgroup.kill"sv,
		"cgroup.max.depth"sv,
		"cgroup.max.descendants"sv,
		"cgroup.pressure"sv,
		"cgroup.procs"sv,
		"cgroup.stat"sv,
		"cgroup.subtree_control"sv,
		"cgroup.threads"sv,
		"cgroup.type"sv,

		"cpu.idle"sv,
		"cpu.max"sv,
		"cpu.max.burst"sv,
		"cpu.pressure"sv,
		"cpu.stat"sv,
		"cpu.stat.local"sv,
		"cpu.weight"sv,
		"cpu.weight.nice"sv,

		"io.bfq.weight"sv,
		"io.latency"sv,
		"io.pressure"sv,
		"io.prio.class"sv,
		"io.stat"sv,
		"io.weight"sv,

		"memory.current"sv,
		"memory.events"sv,
		"memory.events.local"sv,
		"memory.high"sv,
		"memory.low"sv,
		"memory.max"sv,
		"memory.min"sv,
		"memory.numa_stat"sv,
		"memory.oom.group"sv,
		"memory.peak"sv,
		"memory.pressure"sv,
		"memory.reclaim"sv,
		"memory.stat"sv,

		"pids.current"sv,
		"pids.events"sv,
		"pids.events.local"sv,
		"pids.forks"sv,
		"pids.max"sv,
		"pids.peak"sv,
	};

	return std::binary_search(std::begin(skip_names), std::end(skip_names), name);
}

void
UnifiedCgroupWatch::OnDirectoryCreated(const std::string_view relative_path,
				       FileDescriptor directory_fd) noexcept
{
	try {
		/* if this new cgroup was just created, call
		   IsPopulated() to discard the initial event; we
		   don't want to auto-delete if it it's empty, because
		   we already know it's empty; delete empty cgroups
		   immediately only during the initial scan, i.e. from
		   inside AddCgroup() */
		const bool discard = !in_add;

		InsertGroup(relative_path, directory_fd, discard);
	} catch (...) {
		PrintException(std::current_exception());
	}
}

void
UnifiedCgroupWatch::OnDirectoryDeleted(const std::string_view relative_path) noexcept
{
	auto i = groups.find(relative_path);
	if (i == groups.end())
		return;

	groups.erase(i);
}
