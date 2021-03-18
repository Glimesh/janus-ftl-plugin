/**
 * @file FtlControlConnectionUnitTests.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include <memory>

#include "../../src/FtlControlConnection.h"
#include "../mocks/MockConnectionTransport.h"
#include "../mocks/MockFtlControlConnectionManager.h"

TEST_CASE("FtlControlConnection can negotiate valid control connections")
{
    // Fire up a MockFtlControlConnectionManager for the FtlControlConnection to report to
    auto connectionManager = std::make_unique<MockFtlControlConnectionManager>();

    // Fire up a mock connection transport that we'll use to inject fake socket data
    auto mockTransport = std::make_unique<MockConnectionTransport>();
    // Keep track of the raw pointer, since we'll pass unique ownership to the control connection
    // auto mockTransportPtr = mockTransport.get();

    // Spin up our FtlControlConnection
    auto controlConnection = std::make_unique<FtlControlConnection>(connectionManager.get(),
        std::move(mockTransport));
}