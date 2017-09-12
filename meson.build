project('CM4all spawn', ['c', 'cpp'], version: '0.3',
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

libevent = dependency('libevent', version: '>= 2.0.19')
libsystemd = dependency('libsystemd')
libdbus = dependency('dbus-1')

inc = include_directories('src', 'libcommon/src')

util = static_library('util',
  'libcommon/src/util/AllocatedString.cxx',
  'libcommon/src/util/Exception.cxx',
  'libcommon/src/util/LeakDetector.cxx',
  'libcommon/src/util/PrintException.cxx',
  'libcommon/src/util/StringBuilder.cxx',
  'libcommon/src/util/StringParser.cxx',
  'libcommon/src/util/StringUtil.cxx',
  'libcommon/src/util/StringView.cxx',
  'libcommon/src/util/djbhash.c',
  include_directories: inc,
  dependencies: [
  ])
util_dep = declare_dependency(link_with: util)

system = static_library('system',
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
  'libcommon/src/event/DeferEvent.cxx',
  'libcommon/src/event/SignalEvent.cxx',
  'libcommon/src/event/PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

odbus = static_library('odbus',
  'libcommon/src/odbus/Error.cxx',
  'libcommon/src/odbus/Connection.cxx',
  'libcommon/src/odbus/Message.cxx',
  'libcommon/src/odbus/Watch.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
  ])
odbus_dep = declare_dependency(link_with: odbus)

spawn = static_library('spawn',
  'libcommon/src/spawn/Systemd.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    libdbus,
    odbus_dep,
  ])
spawn_dep = declare_dependency(link_with: spawn)

executable('cm4all-spawn',
  'src/Main.cxx',
  'src/Instance.cxx',
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
    spawn_dep,
  ],
  install: true,
  install_dir: 'sbin')

shared_library('nss_cm4all_logname',
  'src/NssLogname.cxx',
  include_directories: inc,
  install: true,
  install_dir: '/' + get_option('libdir'),
  soversion: '2',
)
