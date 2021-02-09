project(
    'janus_ftl',
    'cpp',
    default_options: [
        'cpp_std=c++2a', # C++20 standard
        'b_lundef=false', # Don't fail over undefined references, since we refer to some Janus code
        'b_asneeded=false', # Don't exclude libraries if we don't reference them directly
        'werror=true', # treat warnings as errors
        'default_library=static',
    ],
)

# Allow unknown pragmas, since we tend to use `pragma region/endregion`
add_project_arguments('-Wno-unknown-pragmas', language: 'cpp') 

# Tell cpp-httplib that we want HTTPS support
add_project_arguments('-DCPPHTTPLIB_OPENSSL_SUPPORT', language: 'cpp') 

# Tell spdlog that we're including our own copy of fmt
add_project_arguments('-DSPDLOG_FMT_EXTERNAL', language: 'cpp')

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
    # Preview Generators
    'src/PreviewGenerators/H264PreviewGenerator.cpp',
    # Service Connections
    'src/ServiceConnections/DummyServiceConnection.cpp',
    'src/ServiceConnections/EdgeNodeServiceConnection.cpp',
    'src/ServiceConnections/GlimeshServiceConnection.cpp',
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

deps = [
    dependency('glib-2.0'),
    dependency('libsrtp2'),
    dependency('jansson'),
    dependency('libssl'),
    dependency('libcrypto'),
    dependency('libavcodec'),
    # Meson wrapped dependencies
    subproject('fmt', default_options: 'default_library=static').get_variable('fmt_dep'),
    subproject('spdlog').get_variable('spdlog_dep'),
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