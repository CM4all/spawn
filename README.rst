spawn
=====

*spawn* is a daemon which provides services to other daemons wishing
to spawn child processes


Building spawn
--------------

You need:

- a C++14 compliant compiler (e.g. gcc or clang)
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `D-Bus <https://www.freedesktop.org/wiki/Software/dbus/>`__
- `Meson 0.37 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Run ``meson``::

 meson . output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us
