project('janus_ftl', 'cpp', default_options:['b_lundef=false', 'b_asneeded=false'])

januspath = get_variable('JANUS_PATH', '/opt/janus')
janusincludepath = get_variable('JANUS_INC_PATH', (januspath + '/include/janus'))
installdir = get_variable('INSTALL_PATH', (januspath + '/lib/janus/plugins'))

sources = files([
    'DummyServiceConnection.cpp',
    'FtlStream.cpp',
    'FtlStreamStore.cpp',
    'GlimeshServiceConnection.cpp',
    'IngestConnection.cpp',
    'IngestServer.cpp',
    'janus_ftl.cpp',
    'JanusSession.cpp',
    'JanusFtl.cpp',
    'RelayThreadPool.cpp'
])

deps = [
    dependency('glib-2.0'),
    dependency('libsrtp2'),
    dependency('openssl'),
    dependency('jansson')
]

incdir = include_directories(
    janusincludepath,
    './cpp-httplib'
)

cppargs = '-Wno-unknown-pragmas'

shared_library(
    'janus_ftl',
    sources,
    dependencies: deps,
    include_directories: incdir,
    cpp_args: cppargs,
    install: true,
    install_dir: installdir
)