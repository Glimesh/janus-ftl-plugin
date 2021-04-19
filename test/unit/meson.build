sources = files([
    # Entrypoint
    '../test.cpp',
    # Unit tests
    'FtlControlConnectionUnitTests.cpp',
    'Rtp/ExtendedSequenceCounterTests.cpp',
    # Project sources
    '../../src/FtlControlConnection.cpp',
    '../../src/FtlStream.cpp',
    '../../src/Rtp/ExtendedSequenceCounter.cpp',
    '../../src/Rtp/RtpPacket.cpp',
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
