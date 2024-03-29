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
    'src/Utilities/Watchdog.cpp',
    # Video Decoders
    'src/VideoDecoders/H264VideoDecoder.cpp',
    # RTP Utilities
    'src/Rtp/ExtendedSequenceCounter.cpp',
    'src/Rtp/RtpPacket.cpp',
    'src/Rtp/SequenceTracker.cpp',
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
    'src/FtlMediaConnection.cpp',
    'src/FtlServer.cpp',
    'src/FtlStream.cpp',
    'src/JanusFtl.cpp',
    'src/JanusSession.cpp',
    'src/JanusStream.cpp',
    # Library entrypoint
    'src/entrypoint.cpp',
])

# Pull in subprojects
cpp_httplib_wrap = subproject('cpp-httplib')
fmt_wrap = subproject('fmt', default_options: 'default_library=static')
meson.override_dependency('fmt', fmt_wrap.get_variable('fmt_dep')) # Use our copy of fmt for spdlog
spdlog_wrap = subproject('spdlog', default_options: ['default_library=static', 'compile_library=true', 'external_fmt=true'] )
catch2_wrap = subproject('catch2')

# Optional libsystemd dep for watchdog monitoring support
systemd_dep = dependency('libsystemd', required : get_option('systemd_watchdog_support'))
if systemd_dep.found()
  add_project_arguments('-DSYSTEMD_WATCHDOG_SUPPORT', language: 'cpp') 
endif

if get_option('janus_playout_delay_support')
    add_project_arguments('-DJANUS_PLAYOUT_DELAY_SUPPORT', language: 'cpp')
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
    cpp_httplib_wrap.get_variable('cpp_httplib_dep'),
    fmt_wrap.get_variable('fmt_dep'),
    spdlog_wrap.get_variable('spdlog_dep'),
    systemd_dep,
]

incdir = include_directories(
    './vendor/janus-ftl-orchestrator/inc',
    './vendor/eventpp/include',
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

subdir('test')
