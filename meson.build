project('CM4all spawn', 'cpp', version: '0.26',
  meson_version: '>= 1.2',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',

    'fmt:cpp_std=c++23',
  ],
)

compiler = meson.get_compiler('cpp')

conf = configuration_data()

debug = get_option('b_ndebug') != 'true'

common_flags = [
  '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wmissing-format-attribute',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wundef',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

add_global_link_arguments(
  compiler.get_supported_link_arguments(
    # make relocations read-only (hardening)
    '-Wl,-z,relro',

    # no lazy binding, please - not worth it for a daemon
    '-Wl,-z,now',
  ),
  language: 'cpp'
)

if not debug
  test_common_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  add_global_link_arguments(
    compiler.get_supported_link_arguments(
      '-Wl,--gc-sections',
      '-Wl,--icf=all',
    ),
    language: 'cpp'
  )
endif

test_global_cxxflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

inc = include_directories(
  'src',
  'libcommon/src',
)

libsystemd = dependency('libsystemd', required: get_option('systemd'))

libcommon_enable_DefaultFifoBuffer = false
libcommon_enable_spawn_server = false
libcommon_enable_spawn_local = false
libcommon_enable_spawn_daemon = true
libcommon_enable_spawn_init = true

libcommon_require_cap = get_option('cap')
libcommon_enable_seccomp = get_option('seccomp')
libcommon_require_sodium = get_option('sodium')
libcommon_enable_libsystemd = libsystemd.found()

subdir('libcommon/src/util')
subdir('libcommon/src/time')
subdir('libcommon/src/lib/cap')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/lib/sodium')
subdir('libcommon/src/io')
subdir('libcommon/src/io/linux')
subdir('libcommon/src/system')

system2 = static_library(
  'system2',
  'src/system/SetupProcess.cxx',
  include_directories: inc,
)
system_dep = declare_dependency(
  link_with: system2,
  dependencies: [
    system_dep,
  ],
)

subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/event/net')
subdir('libcommon/src/spawn')
subdir('libcommon/src/lua')
subdir('libcommon/src/lua/io')

libcommon_require_pg = get_option('pg')
subdir('libcommon/src/pg')
if pg_dep.found()
  subdir('libcommon/src/lua/pg')
  pg_dep = lua_pg_dep
endif

if sodium_dep.found()
  subdir('libcommon/src/lua/sodium')
else
  lua_sodium_dep = sodium_dep
endif

conf.set('HAVE_LIBSODIUM', sodium_dep.found())
conf.set('HAVE_LIBSYSTEMD', libsystemd.found())
conf.set('HAVE_PG', pg_dep.found())
configure_file(output: 'config.h', configuration: conf)

executable('cm4all-spawn-accessory',
  'src/accessory/Main.cxx',
  'src/accessory/Instance.cxx',
  'src/accessory/Namespace.cxx',
  'src/accessory/NamespaceMap.cxx',
  'src/accessory/Connection.cxx',
  'src/accessory/Request.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    event_dep,
    system_dep,
    io_dep,
    event_net_dep,
    spawn_dep,
    util_dep,
    fmt_dep,
  ],
  install: true,
  install_dir: 'sbin')

executable('cm4all-spawn-reaper',
  'src/reaper/Main.cxx',
  'src/reaper/Instance.cxx',
  'src/reaper/Scopes.cxx',
  'src/reaper/Released.cxx',
  'src/reaper/CgroupAccounting.cxx',
  'src/reaper/TreeWatch.cxx',
  'src/reaper/UnifiedWatch.cxx',
  'src/reaper/LInit.cxx',
  'src/reaper/LAccounting.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    event_dep,
    system_dep,
    io_dep,
    lua_dep,
    lua_io_dep,
    pg_dep,
    lua_sodium_dep,
    util_dep,
    time_dep,
    fmt_dep,
  ],
  install: true,
  install_dir: 'sbin')

executable('cm4all-spawn-client',
  'src/Client.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    spawn_dep,
    fmt_dep,
  ])

subdir('test')
subdir('doc')
