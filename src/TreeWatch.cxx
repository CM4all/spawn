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

#include "TreeWatch.hxx"
#include "system/Error.hxx"
#include "system/LinuxFD.hxx"
#include "io/DirectoryReader.hxx"
#include "io/Open.hxx"
#include "util/BindMethod.hxx"
#include "util/IterableSplitString.hxx"
#include "util/PrintException.hxx"

#include <assert.h>
#include <fcntl.h>
#include <sys/inotify.h>

inline
TreeWatch::Directory::Directory(Root, const char *path)
	:parent(nullptr),
	 name(path),
	 fd(OpenPath(path, O_DIRECTORY)),
	 persist(true), all(false)
{
}

inline
TreeWatch::Directory::Directory(Directory &_parent, const std::string &_name,
				bool _persist, bool _all)
	:parent(&_parent), name(_name),
	 persist(_persist), all(_all)
{
}

std::string
TreeWatch::Directory::GetPath() const noexcept
{
	if (parent == nullptr)
		return name;

	return parent->GetPath() + "/" + name;
}

std::string
TreeWatch::Directory::GetRelativePath() const noexcept
{
	if (parent == nullptr)
		return {};

	auto p = parent->GetRelativePath();
	if (!p.empty())
		p.push_back('/');

	return std::move(p) + name;
}

void
TreeWatch::Directory::Open(FileDescriptor parent_fd)
{
	assert(parent_fd.IsDefined());
	assert(!fd.IsDefined());
	assert(watch_descriptor < 0);

	fd = OpenPath(parent_fd, name.c_str(), O_DIRECTORY);
}

inline int
TreeWatch::Directory::AddWatch(FileDescriptor inotify_fd_)
{
	assert(watch_descriptor < 0);

	const auto path = GetPath();
	watch_descriptor = inotify_add_watch(inotify_fd_.Get(), path.c_str(),
					     IN_DONT_FOLLOW|IN_EXCL_UNLINK|IN_ONLYDIR|
					     IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO);
	if (watch_descriptor < 0)
		throw FormatErrno("inotify_add_watch('%s') failed",
				  path.c_str());

	return watch_descriptor;
}

TreeWatch::TreeWatch(EventLoop &event_loop, const char *_base_path)
	:inotify_fd(CreateInotify()),
	 inotify_event(event_loop, BIND_THIS_METHOD(OnInotifyEvent),
		       SocketDescriptor::FromFileDescriptor(inotify_fd)),
	 root(Directory::Root(), _base_path)
{
	AddWatch(root);

	inotify_event.ScheduleRead();
}

void
TreeWatch::Add(const char *relative_path)
{
	assert(relative_path != nullptr);
	assert(root.IsOpen());

	Directory *directory = &root;

	for (const auto name : IterableSplitString(relative_path, '/')) {
		if (name.empty())
			continue;

		auto &child = MakeChild(*directory, {name.data, name.size},
					true, false);

		if (!child.IsOpen() && directory->IsOpen()) {
			try {
				child.Open(directory->fd);
				AddWatch(child);
			} catch (...) {
				PrintException(std::current_exception());
			}
		}

		directory = &child;
	}

	if (directory != &root) {
		directory->all = true;

		if (directory->IsOpen() && directory->children.empty()) {
			OnDirectoryCreated(directory->GetRelativePath(),
					   directory->fd);
			ScanDirectory(*directory);
		}
	}
}

TreeWatch::Directory &
TreeWatch::MakeChild(Directory &parent, const std::string &name,
		     bool persist, bool all)
{
	return parent.children.emplace(std::piecewise_construct,
				       std::forward_as_tuple(name),
				       std::forward_as_tuple(parent,
							     name,
							     persist,
							     all))
		.first->second;
}

void
TreeWatch::AddWatch(Directory &directory)
{
	assert(directory.IsOpen());

	[[maybe_unused]] auto i =
		watch_descriptor_map.emplace(directory.AddWatch(inotify_fd),
					     &directory);
	assert(i.second);
}

inline void
TreeWatch::RemoveWatch(int wd) noexcept
{
	assert(wd >= 0);

	auto i = watch_descriptor_map.find(wd);
	assert(i != watch_descriptor_map.end());
	watch_descriptor_map.erase(i);

	inotify_rm_watch(inotify_fd.Get(), wd);
}

void
TreeWatch::ScanDirectory(Directory &directory)
{
	assert(directory.IsOpen());
	assert(directory.watch_descriptor >= 0);
	assert(directory.children.empty());

	DirectoryReader reader(OpenDirectory(directory.fd, "."));
	while (const char *name = reader.Read()) {
		if (*name == '.')
			continue;

		try {
			auto fd = OpenPath(directory.fd, name, O_DIRECTORY);

			auto &child = MakeChild(directory, name, false, true);
			if (child.IsOpen())
				continue;

			assert(child.children.empty());

			child.fd = std::move(fd);
			AddWatch(child);

			OnDirectoryCreated(child.GetRelativePath(), child.fd);

			ScanDirectory(child);
		} catch (const std::system_error &e) {
			if (e.code().category() == ErrnoCategory() &&
			    e.code().value() == ENOTDIR)
				continue;

			PrintException(std::current_exception());
		} catch (...) {
			PrintException(std::current_exception());
		}
	}
}

void
TreeWatch::HandleDeletedDirectory(Directory &directory) noexcept
{
	if (directory.all)
		OnDirectoryDeleted(directory.GetRelativePath());

	directory.fd.Close();

	if (directory.watch_descriptor >= 0)
		RemoveWatch(std::exchange(directory.watch_descriptor, -1));

	for (auto i = directory.children.begin(), end = directory.children.end(); i != end;) {
		auto &child = i->second;

		HandleDeletedDirectory(child);

		assert(child.children.empty() || child.persist);
		assert(!child.persist || directory.persist);

		if (child.persist)
			++i;
		else
			i = directory.children.erase(i);
	}
}

void
TreeWatch::HandleNewDirectory(Directory &parent, std::string &&name)
{
	assert(parent.IsOpen());

	Directory *child;

	if (parent.all) {
		child = &MakeChild(parent, std::move(name), false, true);
	} else {
		auto i = parent.children.find(name);
		if (i == parent.children.end())
			return;

		child = &i->second;
	}

	if (!child->IsOpen()) {
		child->Open(parent.fd);
		AddWatch(*child);

		OnDirectoryCreated(child->GetRelativePath(), child->fd);

		if (child->all)
			ScanDirectory(*child);
	}
}

void
TreeWatch::HandleDeletedDirectory(Directory &parent,
				  std::string &&name) noexcept
{
	auto i = parent.children.find(name);
	if (i == parent.children.end())
		return;

	auto &child = i->second;

	HandleDeletedDirectory(child);

	if (!child.persist)
		parent.children.erase(i);
}

inline void
TreeWatch::HandleInotifyEvent(Directory &directory, uint32_t mask,
			      std::string &&name) noexcept
{
	try {
		if (mask & (IN_CREATE|IN_MOVED_TO))
			HandleNewDirectory(directory, std::move(name));
		else if (mask & (IN_DELETE|IN_MOVED_FROM))
			HandleDeletedDirectory(directory, std::move(name));
	} catch (...) {
		fprintf(stderr, "Failed to handle inotify event 0x%x on '%s/%s': ",
			unsigned(mask),
			directory.GetPath().c_str(), name.c_str());
		PrintException(std::current_exception());
	}
}

inline void
TreeWatch::HandleInotifyEvent(Directory &directory,
			      const struct inotify_event &event) noexcept
{
	if ((event.mask & (IN_ISDIR|IN_IGNORED)) != IN_ISDIR)
		return;

	if (event.len == 0 || event.name[event.len - 1] != 0)
		return;

	HandleInotifyEvent(directory, event.mask, event.name);
}

void
TreeWatch::OnInotifyEvent(unsigned) noexcept
{
	uint8_t buffer[1024];
	ssize_t nbytes = inotify_fd.Read(buffer, sizeof(buffer));
	if (nbytes < 0) {
		if (errno == EAGAIN)
			return;

		perror("Reading from inotify failed");
		inotify_event.Cancel();
		return;
	}

	if (nbytes == 0) {
		fprintf(stderr, "EOF from inotify\n");
		inotify_event.Cancel();
		return;
	}

	const uint8_t *const end = buffer + nbytes;
	const uint8_t *p = buffer;
	const struct inotify_event *event;

	while (p + sizeof(*event) <= end) {
		event = (const struct inotify_event *)(const void *)p;
		auto i = watch_descriptor_map.find(event->wd);
		if (i != watch_descriptor_map.end())
			HandleInotifyEvent(*i->second, *event);

		p = (const uint8_t *)(event + 1) + event->len;
	}
}
