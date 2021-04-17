sources = files([
    '../../test.cpp',
    '../../../src/Rtp/ExtendedSequenceCounter.cpp',
    'ExtendedSequenceCounterTest.cpp',
])

exe = executable(
    'unit-rtp-tests',
    sources,
    dependencies: [test_deps],
    include_directories: [],
)
test('unit-rtp-tests', exe)
