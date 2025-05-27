// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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
TreeWatch::Directory::Directory(Root, TreeWatch &_tree_watch, FileDescriptor directory_fd,
				const char *path)
	:InotifyWatch(_tree_watch.inotify_manager), tree_watch(_tree_watch),
	 parent(nullptr),
	 fd(OpenPath(directory_fd, path, O_DIRECTORY)),
	 persist(true), all(false)
{
}

inline
TreeWatch::Directory::Directory(Directory &_parent, std::string_view _name,
				bool _persist, bool _all) noexcept
	:InotifyWatch(_parent.tree_watch.inotify_manager), tree_watch(_parent.tree_watch),
	 parent(&_parent), name(_name),
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
	assert(!IsWatching());

	fd = OpenPath(parent_fd, name.c_str(), O_DIRECTORY);
}

inline void
TreeWatch::Directory::AddWatch()
{
	assert(!IsWatching());

	InotifyWatch::AddWatch(ProcFdPath(fd),
			       IN_EXCL_UNLINK|IN_ONLYDIR|
			       IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO);
}

void
TreeWatch::Directory::OnInotify(unsigned mask, const char *_name) noexcept
{
	tree_watch.HandleInotifyEvent(*this, mask, _name);
}

TreeWatch::TreeWatch(EventLoop &event_loop, FileDescriptor directory_fd,
		     const char *base_path)
	:inotify_manager(event_loop),
	 root(Directory::Root(), *this, directory_fd, base_path)
{
	root.AddWatch();
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
				child.AddWatch();
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
TreeWatch::ScanDirectory(Directory &directory)
{
	assert(directory.IsOpen());
	assert(directory.IsWatching());
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
			child.AddWatch();

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
	directory.RemoveWatch();

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
		child->AddWatch();

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
