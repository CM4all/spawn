// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "TreeWatch.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "system/Error.hxx"
#include "io/DirectoryReader.hxx"
#include "io/Open.hxx"
#include "io/linux/ProcPath.hxx"
#include "util/IterableSplitString.hxx"
#include "util/PrintException.hxx"

#include <assert.h>
#include <fcntl.h>
#include <sys/inotify.h>

inline
TreeWatch::Directory::Directory(Root, FileDescriptor directory_fd,
				const char *path)
	:parent(nullptr),
	 fd(OpenPath(directory_fd, path, O_DIRECTORY)),
	 persist(true), all(false)
{
}

inline
TreeWatch::Directory::Directory(Directory &_parent, std::string_view _name,
				bool _persist, bool _all) noexcept
	:parent(&_parent), name(_name),
	 persist(_persist), all(_all)
{
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
TreeWatch::Directory::AddWatch(InotifyEvent &ie)
{
	assert(watch_descriptor < 0);

	watch_descriptor =
		ie.AddWatch(ProcFdPath(fd),
			    IN_EXCL_UNLINK|IN_ONLYDIR|
			    IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO);

	return watch_descriptor;
}

TreeWatch::TreeWatch(EventLoop &event_loop, FileDescriptor directory_fd,
		     const char *base_path)
	:inotify_event(event_loop, *this),
	 root(Directory::Root(), directory_fd, base_path)
{
	AddWatch(root);
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

		auto &child = MakeChild(*directory, name, true, false);

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
TreeWatch::MakeChild(Directory &parent, std::string_view name,
		     bool persist, bool all) noexcept
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

	directory.AddWatch(inotify_event);
	watch_descriptor_map.insert(directory);
}

inline void
TreeWatch::RemoveWatch(Directory &directory) noexcept
{
	assert(directory.watch_descriptor >= 0);

	watch_descriptor_map.erase(watch_descriptor_map.iterator_to(directory));
	inotify_event.RemoveWatch(std::exchange(directory.watch_descriptor, -1));
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
			if (IsPathNotFound(e))
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
		RemoveWatch(directory);

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
TreeWatch::HandleNewDirectory(Directory &parent, std::string_view name)
{
	assert(parent.IsOpen());

	Directory *child;

	if (parent.all) {
		child = &MakeChild(parent, name, false, true);
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
				  std::string_view name) noexcept
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
			      std::string_view name) noexcept
{
	try {
		if (mask & (IN_CREATE|IN_MOVED_TO))
			HandleNewDirectory(directory, name);
		else if (mask & (IN_DELETE|IN_MOVED_FROM))
			HandleDeletedDirectory(directory, name);
	} catch (...) {
		fmt::print(stderr, "Failed to handle inotify event {:#x} on '{}/{}': {}\n",
			   mask, directory.GetRelativePath(), name,
			   std::current_exception());
	}
}

inline void
TreeWatch::HandleInotifyEvent(Directory &directory,
			      unsigned mask, const char *name) noexcept
{
	if ((mask & (IN_ISDIR|IN_IGNORED)) != IN_ISDIR)
		return;

	if (name == nullptr)
		return;

	HandleInotifyEvent(directory, mask, std::string_view{name});
}

void
TreeWatch::OnInotify(int wd, unsigned mask, const char *name)
{
	if (auto i = watch_descriptor_map.find(wd);
	    i != watch_descriptor_map.end())
		HandleInotifyEvent(*i, mask, name);
}

void
TreeWatch::OnInotifyError(std::exception_ptr error) noexcept
{
	PrintException(error);
}
