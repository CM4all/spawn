spawn
=====

*spawn* is a daemon which provides services to other daemons wishing
to spawn child processes

For more information, `read the manual
<https://cm4all-spawn.readthedocs.io/en/latest/>`__ in the ``doc``
directory.


Building spawn
--------------

You need:

- a C++20 compliant compiler (e.g. gcc or clang)
- `Meson 1.2 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__
- `libfmt <https://fmt.dev/>`__
- `LuaJIT <http://luajit.org/>`__

Optional dependencies:

- `libcap2 <https://sites.google.com/site/fullycapable/>`__ for
  dropping unnecessary Linux capabilities
- `libseccomp <https://github.com/seccomp/libseccomp>`__ for system
  call filter support
- `libsodium <https://www.libsodium.org/>`__
- `libpq <https://www.postgresql.org/>`__ for PostgreSQL support in
  Lua code
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__

Get the source code::

 git clone --recursive https://github.com/CM4all/spawn.git

Run ``meson``::

 meson setup output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us
