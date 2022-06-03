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

#include "io/UniqueFileDescriptor.hxx"
#include "event/SocketEvent.hxx"

#include <map>
#include <string>

class TreeWatch {
	UniqueFileDescriptor inotify_fd;
	SocketEvent inotify_event;

	struct Directory {
		Directory *const parent;

		const std::string name;

		UniqueFileDescriptor fd;

		std::map<std::string, Directory> children;

		int watch_descriptor = -1;

		bool persist;
		bool all;

		struct Root {};

		Directory(Root, const char *path);

		Directory(Directory &_parent, const std::string &_name,
			  bool _persist, bool _all);

		std::string GetPath() const noexcept;
		std::string GetRelativePath() const noexcept;

		bool IsOpen() const noexcept {
			return fd.IsDefined();
		}

		void Open(FileDescriptor parent_fd);

		int AddWatch(FileDescriptor inotify_fd);
	};

	Directory root;

	std::map<int, Directory *> watch_descriptor_map;

public:
	TreeWatch(EventLoop &event_loop, const char *base_path);

	auto &GetEventLoop() const noexcept {
		return inotify_event.GetEventLoop();
	}

	const std::string &GetBasePath() noexcept {
		return root.name;
	}

	void Add(const char *relative_path);

private:
	Directory &MakeChild(Directory &parent, const std::string &name,
			     bool persist, bool all);

	void AddWatch(Directory &directory);
	void RemoveWatch(int wd) noexcept;

	void ScanDirectory(Directory &directory);

	void HandleNewDirectory(Directory &parent, std::string &&name);

	void HandleDeletedDirectory(Directory &directory) noexcept;
	void HandleDeletedDirectory(Directory &parent,
				    std::string &&name) noexcept;

	void HandleInotifyEvent(Directory &directory, uint32_t mask,
				std::string &&name) noexcept;
	void HandleInotifyEvent(Directory &directory,
				const struct inotify_event &event) noexcept;

	void OnInotifyEvent(unsigned events) noexcept;

protected:
	virtual void OnDirectoryCreated(const std::string &relative_path,
					FileDescriptor directory_fd) noexcept = 0;
	virtual void OnDirectoryDeleted(const std::string &relative_path) noexcept = 0;
};
