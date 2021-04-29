test_incdirs = include_directories(
    '../vendor/eventpp/include',
    is_system: true,
)

test_deps = [
    dependency('libssl'),
    dependency('libcrypto'),
    fmt_wrap.get_variable('fmt_dep'),
    spdlog_wrap.get_variable('spdlog_dep'),
    catch2_wrap.get_variable('catch2_dep'),
]

subdir('unit')
subdir('functional')