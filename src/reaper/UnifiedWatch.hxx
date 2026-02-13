// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "TreeWatch.hxx"

#include <map>
#include <string>

/**
 * Watch events in the "unified" (v2) cgroup hierarchy.
 */
class UnifiedCgroupWatch final : TreeWatch {
	typedef BoundMethod<void(const char *relative_path) noexcept> Callback;
	const Callback callback;

	class Group;

	std::map<std::string, Group, std::less<>> groups;

	bool in_add = false;

public:
	UnifiedCgroupWatch(EventLoop &event_loop, FileDescriptor cgroup2_mount,
			   Callback _callback);
	~UnifiedCgroupWatch() noexcept;

	void AddCgroup(std::string_view relative_path);

	/**
	 * Re-add a cgroup that is still registered in #TreeWatch.
	 * This can be used after the rmdir() has failed with EBUSY
	 * because somebody has spawned a new process into the cgroup
	 * before we had a chance to reap it.
	 */
	void ReAddCgroup(std::string_view relative_path) noexcept;

private:
	/**
	 * Insert a new #Group instance into the #groups map.
	 *
	 * Throws on error.
	 *
	 * @param directory_fd the cgorup directory #FileDescriptor
	 * @param discard true to discard the initial event
	 */
	void InsertGroup(const std::string_view relative_path,
			 FileDescriptor directory_fd,
			 bool discard);

	void OnGroupEmpty(Group &group) noexcept;

protected:
	bool ShouldSkipName(std::string_view name) const noexcept override;
	void OnDirectoryCreated(std::string_view relative_path,
				FileDescriptor directory_fd) noexcept override;
	void OnDirectoryEmpty(std::string_view relative_path) noexcept override;
	void OnDirectoryDeleted(std::string_view relative_path) noexcept override;
};
