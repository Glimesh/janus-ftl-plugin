sources = files([
    '../../test.cpp',
    '../../../src/ConnectionTransports/NetworkSocketConnectionTransport.cpp',
    'NetworkSocketConnectionTransportTest.cpp',
])

exe = executable(
    'unit-connectiontransports-tests',
    sources,
    dependencies: [test_deps],
    include_directories: [],
)
test('unit-connectiontransports-tests', exe)
