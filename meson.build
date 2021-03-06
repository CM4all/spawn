project('CM4all spawn', 'cpp', version: '0.8',
  meson_version: '>= 0.47',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
  ],
)

compiler = meson.get_compiler('cpp')

debug = get_option('b_ndebug') != 'true'

common_flags = [
  '-D_GNU_SOURCE',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
]

test_common_flags = [
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

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',
  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

inc = include_directories(
  'src',
  'libcommon/src',
)

libcommon_enable_c = false
libcommon_enable_boost = false
libcommon_enable_DefaultFifoBuffer = false
libcommon_enable_AllocatorPtr = false
libcommon_enable_net_log = false

subdir('libcommon/src/util')
subdir('libcommon/src/io')
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
subdir('libcommon/src/odbus')
subdir('libcommon/src/spawn')

executable('cm4all-spawn',
  'src/Main.cxx',
  'src/Instance.cxx',
  'src/Namespace.cxx',
  'src/Connection.cxx',
  'src/Request.cxx',
  'src/Scopes.cxx',
  'src/Released.cxx',
  'src/TreeWatch.cxx',
  'src/UnifiedWatch.cxx',
  include_directories: inc,
  dependencies: [
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

subdir('test')
