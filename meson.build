project('CM4all spawn', ['c', 'cpp'], version: '0.4',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

common_flags = [
  '-D_GNU_SOURCE',
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-Wundef',
]

if debug
  common_flags += ['-DPOISON']
endif

add_global_arguments(common_flags,
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wnested-externs',
  '-pedantic',
  language: 'c')

add_global_arguments(common_flags,
  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',
  language: 'cpp')

libsystemd = dependency('libsystemd')
libdbus = dependency('dbus-1')
libcap = dependency('libcap')
libseccomp = dependency('libseccomp')

inc = include_directories('src', 'libcommon/src')

util = static_library('util',
  'libcommon/src/util/AllocatedString.cxx',
  'libcommon/src/util/Exception.cxx',
  'libcommon/src/util/LeakDetector.cxx',
  'libcommon/src/util/PrintException.cxx',
  'libcommon/src/util/StringBuilder.cxx',
  'libcommon/src/util/StringParser.cxx',
  'libcommon/src/util/StringStrip.cxx',
  'libcommon/src/util/StringView.cxx',
  'libcommon/src/util/djbhash.c',
  include_directories: inc,
  dependencies: [
  ])
util_dep = declare_dependency(link_with: util)

system = static_library('system',
  'libcommon/src/system/CapabilityState.cxx',
  'libcommon/src/system/ProcessName.cxx',
  'libcommon/src/system/LinuxFD.cxx',
  'libcommon/src/system/EpollFD.cxx',
  'src/system/SetupProcess.cxx',
  include_directories: inc,
  dependencies: [
  ])
system_dep = declare_dependency(link_with: system)

io = static_library('io',
  'libcommon/src/io/FileDescriptor.cxx',
  'libcommon/src/io/WriteFile.cxx',
  'libcommon/src/io/Logger.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ])
io_dep = declare_dependency(link_with: io)

event = static_library('event',
  'libcommon/src/event/Loop.cxx',
  'libcommon/src/event/ShutdownListener.cxx',
  'libcommon/src/event/CleanupTimer.cxx',
  'libcommon/src/event/TimerEvent.cxx',
  'libcommon/src/event/DeferEvent.cxx',
  'libcommon/src/event/SocketEvent.cxx',
  'libcommon/src/event/SignalEvent.cxx',
  'libcommon/src/event/PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

net = static_library('net',
  'libcommon/src/net/SocketAddress.cxx',
  'libcommon/src/net/StaticSocketAddress.cxx',
  'libcommon/src/net/AllocatedSocketAddress.cxx',
  'libcommon/src/net/MaskedSocketAddress.cxx',
  'libcommon/src/net/IPv4Address.cxx',
  'libcommon/src/net/IPv6Address.cxx',
  'libcommon/src/net/HostParser.cxx',
  'libcommon/src/net/AddressInfo.cxx',
  'libcommon/src/net/Resolver.cxx',
  'libcommon/src/net/Parser.cxx',
  'libcommon/src/net/ToString.cxx',
  'libcommon/src/net/SendMessage.cxx',
  'libcommon/src/net/SocketDescriptor.cxx',
  'libcommon/src/net/UniqueSocketDescriptor.cxx',
  'libcommon/src/net/djb/NetstringInput.cxx',
  'libcommon/src/net/djb/NetstringHeader.cxx',
  'libcommon/src/net/djb/NetstringGenerator.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    io_dep,
  ])
net_dep = declare_dependency(link_with: net)

event_net = static_library('event_net',
  'libcommon/src/event/net/ConnectSocket.cxx',
  'libcommon/src/event/net/ServerSocket.cxx',
  'libcommon/src/event/net/UdpListener.cxx',
  'libcommon/src/event/net/djb/NetstringServer.cxx',
  'libcommon/src/event/net/djb/NetstringClient.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    net_dep,
    util_dep,
  ])
event_net_dep = declare_dependency(link_with: event_net)

odbus = static_library('odbus',
  'libcommon/src/odbus/Error.cxx',
  'libcommon/src/odbus/Connection.cxx',
  'libcommon/src/odbus/Message.cxx',
  'libcommon/src/odbus/Watch.cxx',
  'libcommon/src/odbus/ScopeMatch.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
  ])
odbus_dep = declare_dependency(link_with: odbus)

spawn = static_library('spawn',
  'libcommon/src/spawn/SeccompFilter.cxx',
  'libcommon/src/spawn/SyscallFilter.cxx',
  'libcommon/src/spawn/Systemd.cxx',
  'libcommon/src/spawn/Init.cxx',
  'libcommon/src/spawn/daemon/Client.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    libdbus,
    libcap,
    libseccomp,
    odbus_dep,
    io_dep,
    net_dep,
    system_dep,
  ])
spawn_dep = declare_dependency(link_with: spawn)

executable('cm4all-spawn',
  'src/Main.cxx',
  'src/Instance.cxx',
  'src/Namespace.cxx',
  'src/Connection.cxx',
  'src/Request.cxx',
  'src/Released.cxx',
  'src/Agent.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    libdbus,
    odbus_dep,
    event_dep,
    system_dep,
    io_dep,
    event_net_dep,
    spawn_dep,
  ],
  install: true,
  install_dir: 'sbin')

executable('cm4all-spawn-client',
  'src/Client.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    spawn_dep,
  ])

shared_library('nss_cm4all_logname',
  'src/NssLogname.cxx',
  include_directories: inc,
  install: true,
  install_dir: '/' + get_option('libdir'),
  soversion: '2',
)
