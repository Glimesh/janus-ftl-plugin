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

# Set Janus paths from env vars, or sane defaults
januspath = get_variable('JANUS_PATH', '/opt/janus')
janusincludepath = get_variable('JANUS_INC_PATH', (januspath + '/include/janus'))
installdir = get_variable('INSTALL_PATH', (januspath + '/lib/janus/plugins'))

sources = files([
    'src/Configuration.cpp',
    'src/DummyServiceConnection.cpp',
    'src/EdgeNodeServiceConnection.cpp',
    'src/FtlClient.cpp',
    'src/FtlStream.cpp',
    'src/FtlStreamStore.cpp',
    'src/GlimeshServiceConnection.cpp',
    'src/H264PreviewGenerator.cpp',
    'src/IngestConnection.cpp',
    'src/IngestServer.cpp',
    'src/janus_ftl.cpp',
    'src/JanusSession.cpp',
    'src/JanusFtl.cpp',
    'src/RelayThreadPool.cpp'
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
    subproject('spdlog', default_options: 'external_fmt=true').get_variable('spdlog_dep'),
]

incdir = include_directories(
    './src',
    './cpp-httplib',
    './janus-ftl-orchestrator/inc',
    janusincludepath,
)

shared_library(
    'janus_ftl',
    sources,
    dependencies: deps,
    include_directories: incdir,
    install: true,
    install_dir: installdir
)

executable(
    'ftlclienttest',
    files(['src/ftlclienttest.cpp', 'src/FtlClient.cpp']),
    dependencies: deps,
    include_directories: incdir
)