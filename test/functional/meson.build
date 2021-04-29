sources = files([
    # Entrypoint
    '../test.cpp',
    # Utilities
    'Utilities/JanusShims.cpp',
    # Functional tests
    'FunctionalTests.cpp',
    # Project sources
    # TODO: This should all probably be a static lib that's only compiled once
        # Utilities
    '../../src/Utilities/Watchdog.cpp',
        # Video Decoders
    '../../src/VideoDecoders/H264VideoDecoder.cpp',
        # RTP Utilities
    '../../src/Rtp/ExtendedSequenceCounter.cpp',
    '../../src/Rtp/RtpPacket.cpp',
        # Service Connections
    '../../src/ServiceConnections/DummyServiceConnection.cpp',
    '../../src/ServiceConnections/EdgeNodeServiceConnection.cpp',
    '../../src/ServiceConnections/GlimeshServiceConnection.cpp',
    '../../src/ServiceConnections/RestServiceConnection.cpp',
        # Connection Transports
    '../../src/ConnectionTransports/NetworkSocketConnectionTransport.cpp',
        # Connection Listeners
    '../../src/ConnectionListeners/TcpConnectionListener.cpp',
        # Connection Creators
    '../../src/ConnectionCreators/UdpConnectionCreator.cpp',
        # Library
    '../../src/Configuration.cpp',
    '../../src/FtlClient.cpp',
    '../../src/FtlControlConnection.cpp',
    '../../src/FtlMediaConnection.cpp',
    '../../src/FtlServer.cpp',
    '../../src/FtlStream.cpp',
    '../../src/JanusFtl.cpp',
    '../../src/JanusSession.cpp',
    '../../src/JanusStream.cpp',
])

# Set Janus paths from env vars, or sane defaults
januspath = get_variable('JANUS_PATH', '/opt/janus')
janusincludepath = get_variable('JANUS_INC_PATH', (januspath + '/include/janus'))
installdir = get_variable('INSTALL_PATH', (januspath + '/lib/janus/plugins'))

incdirs = include_directories(
    '../../vendor/cpp-httplib',
    '../../vendor/janus-ftl-orchestrator/inc',
    '../../vendor/eventpp/include',
    janusincludepath,
    is_system: true,
)

deps = [
    dependency('glib-2.0'),
    dependency('jansson'),
    dependency('libssl'),
    dependency('libcrypto'),
    dependency('libavcodec'),
    dependency('libavutil'),
    fmt_wrap.get_variable('fmt_dep'),
    spdlog_wrap.get_variable('spdlog_dep'),
    catch2_wrap.get_variable('catch2_dep'),
]

exe = executable(
    'janus-ftl-plugin-functional-test',
    sources,
    cpp_pch: '../../pch/janus_ftl_test_pch.h',
    dependencies: deps,
    include_directories: incdirs,
)
test('functional-test', exe)
