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
