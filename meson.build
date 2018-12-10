project('CM4all spawn', 'cpp', version: '0.4',
  meson_version: '>= 0.47',
  default_options: [
    'cpp_std=c++14'
  ],
)

compiler = meson.get_compiler('cpp')

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

common_flags = [
  '-D_GNU_SOURCE',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-Wundef',
]

test_common_flags = [
  '-fvisibility=hidden',
]

if debug
  common_flags += ['-DPOISON']
else
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
  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

inc = include_directories(
  'src',
  'libcommon/src',
)

libcommon_enable_c = false
libcommon_enable_DefaultFifoBuffer = false
libcommon_enable_AllocatorPtr = false

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
  'src/Agent.cxx',
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
