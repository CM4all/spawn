// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"
#include "event/InotifyManager.hxx"

#include <map>
#include <string>
#include <string_view>

class TreeWatch {
	InotifyManager inotify_manager;

	struct Directory final : InotifyWatch {
		TreeWatch &tree_watch;

		Directory *const parent;

		const std::string name;

		UniqueFileDescriptor fd;

		std::map<std::string, Directory, std::less<>> children;

		const bool persist;
		bool all;

		struct Root {};

		Directory(Root, TreeWatch &_tree_watch, FileDescriptor directory_fd,
			  const char *path);

		Directory(Directory &_parent, std::string_view _name,
			  bool _persist, bool _all) noexcept;

		std::string GetRelativePath() const noexcept;

		bool IsOpen() const noexcept {
			return fd.IsDefined();
		}

		void Open(FileDescriptor parent_fd);

		void AddWatch();

	protected:
		// virtual methods from InotifyWatch
		void OnInotify(unsigned mask, const char *name) noexcept override;
	};

	Directory root;

public:
	TreeWatch(EventLoop &event_loop,
		  FileDescriptor directory_fd, const char *base_path);

	auto &GetEventLoop() const noexcept {
		return inotify_manager.GetEventLoop();
	}

	void Add(const char *relative_path);

private:
	Directory &MakeChild(Directory &parent, std::string_view name,
			     bool persist, bool all) noexcept;

	void ScanDirectory(Directory &directory);

	void HandleNewDirectory(Directory &parent, std::string_view name);

	void HandleDeletedDirectory(Directory &directory) noexcept;
	void HandleDeletedDirectory(Directory &parent,
				    std::string_view name) noexcept;

	void HandleInotifyEvent(Directory &directory, uint32_t mask,
				std::string_view name) noexcept;
	void HandleInotifyEvent(Directory &directory, uint32_t mask,
				const char *name) noexcept;

protected:
	virtual void OnDirectoryCreated(const std::string &relative_path,
					FileDescriptor directory_fd) noexcept = 0;
	virtual void OnDirectoryDeleted(const std::string &relative_path) noexcept = 0;
};
