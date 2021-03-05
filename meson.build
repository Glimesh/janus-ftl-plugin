project(
    'janus_ftl',
    'cpp',
    default_options: [
        'cpp_std=c++2a', # C++20 standard
        'b_lundef=false', # Don't fail over undefined references, since we refer to some Janus code
        'werror=true', # treat warnings as errors
    ],
)

# Allow unknown pragmas, since we tend to use `pragma region/endregion`
add_project_arguments('-Wno-unknown-pragmas', language: 'cpp') 

# Tell cpp-httplib that we want HTTPS support
add_project_arguments('-DCPPHTTPLIB_OPENSSL_SUPPORT', language: 'cpp') 

# Set the DEBUG define if we're a debug build
if get_option('buildtype').to_lower() == 'debug'
    add_project_arguments('-DDEBUG', language : 'cpp')
endif

# Set Janus paths from env vars, or sane defaults
januspath = get_variable('JANUS_PATH', '/opt/janus')
janusincludepath = get_variable('JANUS_INC_PATH', (januspath + '/include/janus'))
installdir = get_variable('INSTALL_PATH', (januspath + '/lib/janus/plugins'))

sources = files([
    # Utilities
    'src/Utilities/Rtp.cpp',
    'src/Utilities/Watchdog.cpp',
    # Preview Generators
    'src/PreviewGenerators/H264PreviewGenerator.cpp',
    # Service Connections
    'src/ServiceConnections/DummyServiceConnection.cpp',
    'src/ServiceConnections/EdgeNodeServiceConnection.cpp',
    'src/ServiceConnections/GlimeshServiceConnection.cpp',
    'src/ServiceConnections/RestServiceConnection.cpp',
    # Connection Transports
    'src/ConnectionTransports/NetworkSocketConnectionTransport.cpp',
    # Connection Listeners
    'src/ConnectionListeners/TcpConnectionListener.cpp',
    # Connection Creators
    'src/ConnectionCreators/UdpConnectionCreator.cpp',
    # Library
    'src/Configuration.cpp',
    'src/FtlClient.cpp',
    'src/FtlControlConnection.cpp',
    'src/FtlServer.cpp',
    'src/FtlStream.cpp',
    'src/JanusSession.cpp',
    'src/JanusFtl.cpp',
    # Library entrypoint
    'src/entrypoint.cpp',
])

# Pull in subprojects
fmt_wrap = subproject('fmt', default_options: 'default_library=static')
meson.override_dependency('fmt', fmt_wrap.get_variable('fmt_dep')) # Use our copy of fmt for spdlog
spdlog_wrap = subproject('spdlog', default_options: ['default_library=static', 'compile_library=true', 'external_fmt=true'] )

# Optional libsystemd dep for watchdog monitoring support
systemd_dep = dependency('libsystemd', required : get_option('systemd_watchdog_support'))
if systemd_dep.found()
  add_project_arguments('-DSYSTEMD_WATCHDOG_SUPPORT', language: 'cpp') 
else
  systemd_dep = disabler()
endif

deps = [
    dependency('glib-2.0'),
    dependency('libsrtp2'),
    dependency('jansson'),
    dependency('libssl'),
    dependency('libcrypto'),
    dependency('libavcodec'),
    dependency('libavutil'),
    # Meson wrapped dependencies
    fmt_wrap.get_variable('fmt_dep'),
    spdlog_wrap.get_variable('spdlog_dep'),
    systemd_dep,
]

incdir = include_directories(
    './cpp-httplib',
    './janus-ftl-orchestrator/inc',
    janusincludepath,
    is_system: true,
)

shared_library(
    'janus_ftl',
    sources,
    cpp_pch: 'pch/janus_ftl_pch.h',
    dependencies: deps,
    include_directories: incdir,
    install: true,
    install_dir: installdir
)

#
# Test binary
#

test_sources = files([
    # Entrypoint
    'test/test.cpp',
])

test_incdirs = include_directories(
    './test',
    './src',
)

test_deps = [
    subproject('spdlog').get_variable('spdlog_dep'),
    subproject('catch2').get_variable('catch2_dep'),
]

executable(
    'janus-ftl-plugin-test',
    test_sources,
    dependencies: test_deps,
    include_directories: test_incdirs,
)