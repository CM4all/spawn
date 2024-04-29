// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"
#include "event/InotifyEvent.hxx"
#include "util/IntrusiveHashSet.hxx"

#include <map>
#include <string>
#include <string_view>

class TreeWatch : InotifyHandler {
	InotifyEvent inotify_event;

	struct Directory final : IntrusiveHashSetHook<> {
		Directory *const parent;

		const std::string name;

		UniqueFileDescriptor fd;

		std::map<std::string, Directory, std::less<>> children;

		int watch_descriptor = -1;

		const bool persist;
		bool all;

		struct Root {};

		Directory(Root, FileDescriptor directory_fd,
			  const char *path);

		Directory(Directory &_parent, std::string_view _name,
			  bool _persist, bool _all) noexcept;

		std::string GetRelativePath() const noexcept;

		bool IsOpen() const noexcept {
			return fd.IsDefined();
		}

		void Open(FileDescriptor parent_fd);

		int AddWatch(InotifyEvent &inotify_event);

		struct GetInotify {
			constexpr int operator()(const Directory &d) noexcept {
				return d.watch_descriptor;
			}
		};
	};

	Directory root;

	/**
	 * Map inotify watch descriptors to #Directory.
	 */
	IntrusiveHashSet<Directory, 256,
			 IntrusiveHashSetOperators<Directory, Directory::GetInotify,
						   std::hash<int>,
						   std::equal_to<int>>> watch_descriptor_map;

public:
	TreeWatch(EventLoop &event_loop,
		  FileDescriptor directory_fd, const char *base_path);

	auto &GetEventLoop() const noexcept {
		return inotify_event.GetEventLoop();
	}

	void Add(const char *relative_path);

private:
	Directory &MakeChild(Directory &parent, std::string_view name,
			     bool persist, bool all) noexcept;

	void AddWatch(Directory &directory);
	void RemoveWatch(Directory &directory) noexcept;

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

private:
	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
