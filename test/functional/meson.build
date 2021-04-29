sources = files([
    # Entrypoint
    '../test.cpp',
    # Functional tests
    'FunctionalTests.cpp'
    # Project sources
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
    'janus-ftl-plugin-functional-test',
    sources,
    cpp_pch: '../../pch/janus_ftl_test_pch.h',
    dependencies: deps,
    include_directories: incdirs,
)
test('functional-test', exe)
