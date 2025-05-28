spawn
=====

What is spawn?
---------------

``spawn`` is a suite which provides services to the process spawner of
various other daemons, e.g. `beng-proxy
<https://github.com/CM4all/beng-proxy/>`__ and `Workshop
<https://github.com/CM4all/workshop/>`__.


The Accessory Daemon
--------------------

This daemon listens for seqpacket connections on abstract socket
:file:`@cm4all-spawn` and allow clients to create namespaces
(`protocol definition
<https://github.com/CM4all/libcommon/blob/master/src/spawn/accessory/Protocol.hxx>`__).
The client is usually the process spawner of daemons like `beng-proxy
<https://github.com/CM4all/beng-proxy/>`__ and `Lukko
<https://github.com/CM4all/lukko/>`__.


The Reaper Daemon
-----------------

This daemon watches cgroups below certain scopes; once they run empty,
statistics are collected and logged and the cgroup is deleted.


``SIGHUP``
^^^^^^^^^^

On ``systemctl reload cm4all-spawn-reaper`` (i.e. ``SIGHUP``), the
daemon calls the Lua function ``reload`` if one was defined.  It is up
to the Lua script to define the exact meaning of this feature.


Resource Accounting
^^^^^^^^^^^^^^^^^^^

.. highlight:: lua

The file :file:`/etc/cm4all/spawn/reaper.lua` is a `Lua
<http://www.lua.org/>`_ script which is executed at startup.  If it
defines a function called ``cgroup_released``, then this function will
be called every time a cgroup is reaped.  The function may for example
log its resource usage.  Example::

  function cgroup_released(cgroup)
    print(cgroup.memory_peak)
  end

The following attributes of the ``cgroup`` parameter can be queried:

* ``path``: the cgroup path as noted in :file:`/proc/self/cgroup`,
  e.g. :file:`/user.slice/user-1000.slice/session-42.scope`

* ``btime``: The time the cgroup was created as `Lua timestamp
  <https://www.lua.org/pil/22.1.html>`__; may be ``nil`` if the kernel
  does not support ``btime`` on ``cgroupfs``.

* ``age``: The age of this cgroup in seconds.  Only available if
  ``btime`` is.

* ``xattr``: A table containing extended attributes of the control
  group.

* ``parent``: Information about the parent of this cgroup; it is
  another object of this type (or ``nil`` if there is no parent
  cgroup).

* ``cpu_total``, ``cpu_user``, ``cpu_system``: the total,
  userspace-only or kernel-only CPU usage [in seconds].

* ``memory_peak``: the peak memory usage [in bytes].

* ``memory_events_high``: The number of times processes of the cgroup
  are throttled and routed to perform direct memory reclaim because
  the high memory boundary was exceeded.

* ``memory_events_max``: The number of times the cgroup's memory usage
  was about to go over the max boundary.

* ``memory_events_oom``: The number of time the cgroup's memory usage
  was reached the limit and allocation was about to fail.

* ``pids_peak``: the peak number of processes.

* ``pids_forks``: the number of ``fork()`` system calls

* ``pids_events_max``: the number of times the ``pids.max`` setting
  was exceeded.


libsodium
^^^^^^^^^

There are some `libsodium <https://www.libsodium.org/>`__ bindings.

`Helpers <https://doc.libsodium.org/helpers>`__::

  bin = sodium.hex2bin("deadbeef") -- returns "\xde\xad\xbe\ef"
  hex = sodium.bin2hex("A\0\xff") -- returns "4100ff"

`Generating random data
<https://doc.libsodium.org/generating_random_data>`__::

  key = sodium.randombytes(32)

`Sealed boxes
<https://libsodium.gitbook.io/doc/public-key_cryptography/sealed_boxes>`__::

  pk, sk = sodium.crypto_box_keypair()
  ciphertext = sodium.crypto_box_seal('hello world', pk)
  message = sodium.crypto_box_seal_open(ciphertext, pk, sk)

`Point*scalar multiplication
<https://doc.libsodium.org/advanced/scalar_multiplication>__::

  pk = sodium.crypto_scalarmult_base(sk)


PostgreSQL Client
^^^^^^^^^^^^^^^^^

The Lua script can query a PostgreSQL database.  First, a connection
should be established during initialization::

  db = pg:new('dbname=foo', 'schemaname')

In the handler function, queries can be executed like this (the API is
similar to `LuaSQL <https://keplerproject.github.io/luasql/>`__)::

  local result = assert(db:execute('SELECT id, name FROM bar'))
  local row = result:fetch({}, "a")
  print(row.id, row.name)

Query parameters are passed to ``db:execute()`` as an array after the
SQL string::

  local result = assert(
    db:execute('SELECT name FROM bar WHERE id=$1', {42}))

The functions ``pg:encode_array()`` and ``pg:decode_array()`` support
PostgreSQL arrays; the former encodes a Lua array to a PostgreSQL
array string, and the latter decodes a PostgreSQL array string to a
Lua array.

To listen for `PostgreSQL notifications
<https://www.postgresql.org/docs/current/sql-notify.html>`__, invoke
the ``listen`` method with a callback function::

  db:listen('bar', function()
    print("Received a PostgreSQL NOTIFY")
  end)


Network Namespaces
------------------

The Debian package :file:`cm4all-spawn-netns` contains the systemd
service template :file:`cm4all-spawn-netns@.service` which creates a
new network namespace connected with the current namespace over a pair
of ``veth`` devices.  This requires a script in
:file:`/etc/cm4all/spawn/netns/setup.d` which sets up the ``veth``
device inside the new namespace; its name is passed as command-line
argument.  The other ``veth`` device is expected to be set up with
:file:`systemd-networkd`.


Slice
-----

The Debian package :file:`cm4all-slice` contains the systemd slice
``system-cm4all.slice`` where the scopes of most process spawners
live.


NSS-LogName
-----------

The NSS module was moved to https://github.com/CM4all/nss_logname
