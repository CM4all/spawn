spawn
=====

What is spawn?
---------------

``spawn`` is a suite which provides services to the process spawner of
various other daemons, e.g. `beng-proxy
<https://github.com/CM4all/beng-proxy/>`__ and `Workshop
<https://github.com/CM4all/workshop/>`__.


The ``spawn`` Daemon
--------------------

The daemon fulfills the following duties:

- watch cgroups below certain scopes; once they run empty, statistics
  are collected and logged and the cgroup is deleted
- listen for seqpacket connections on abstract socket
  :file:`@cm4all-spawn` and allow clients to create namespaces
  (`protocol definition <https://github.com/CM4all/libcommon/blob/master/src/spawn/daemon/Protocol.hxx>`__)

Resource Accounting
^^^^^^^^^^^^^^^^^^^

.. highlight:: lua

The file :file:`/etc/cm4all/spawn/accounting.lua` is a `Lua
<http://www.lua.org/>`_ script which is executed at startup.  If it
defines a function called ``cgroup_released``, then this function will
be called every time a cgroup is reaped.  The function may for example
log its resource usage.  Example::

  function cgroup_released(cgroup)
    print(cgroup.memory_max_usage)
  end

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

The Debian package :file:`libnss-cm4all-logname` contains a glibc NSS
module which pretends there is a :file:`/etc/passwd` entry for the
current uid named ``$LOGNAME``.  This should be installed in
containers spawned by the process spawner.
