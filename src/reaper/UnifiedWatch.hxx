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

private:
	void OnGroupEmpty(Group &group) noexcept;

protected:
	void OnDirectoryCreated(const std::string &relative_path,
				FileDescriptor directory_fd) noexcept override;
	void OnDirectoryDeleted(const std::string &relative_path) noexcept override;
};
