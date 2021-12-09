sources = files([
    # Entrypoint
    '../test.cpp',
    # Unit tests
    'ConnectionTransports/NetworkSocketConnectionTransportTests.cpp',
    'FtlControlConnectionUnitTests.cpp',
    'Rtp/ExtendedSequenceCounterTests.cpp',
    'Utilities/UtilTest.cpp',
    # Project sources
    '../../src/ConnectionTransports/NetworkSocketConnectionTransport.cpp',
    '../../src/FtlControlConnection.cpp',
    '../../src/FtlMediaConnection.cpp',
    '../../src/FtlStream.cpp',
    '../../src/Rtp/ExtendedSequenceCounter.cpp',
    '../../src/Rtp/RtpPacket.cpp',
    '../../src/Rtp/SequenceTracker.cpp',
])

incdirs = include_directories(
    '../../vendor/eventpp/include',
    is_system: true,
)

deps = [
    dependency('libssl'),
    dependency('libcrypto'),
    fmt_wrap.get_variable('fmt_dep'),
    spdlog_wrap.get_variable('spdlog_dep'),
    catch2_wrap.get_variable('catch2_dep'),
]

exe = executable(
    'janus-ftl-plugin-unit-test',
    sources,
    cpp_pch: '../../pch/janus_ftl_test_pch.h',
    dependencies: deps,
    include_directories: incdirs,
)
test('unit-test', exe)
