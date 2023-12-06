spawn
=====

*spawn* is a daemon which provides services to other daemons wishing
to spawn child processes


Building spawn
--------------

You need:

- a C++20 compliant compiler (e.g. gcc or clang)
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `Meson 0.56 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

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
