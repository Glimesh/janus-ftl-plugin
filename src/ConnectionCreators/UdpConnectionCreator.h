/**
 * @file UdpConnectionCreator.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-05
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "ConnectionCreator.h"

/**
 * @brief Creates UdpConnectionTransports!
 */
class UdpConnectionCreator : public ConnectionCreator
{
public:
    // ConnectionCreator implementation
    std::unique_ptr<ConnectionTransport> CreateConnection(
        int port,
        sockaddr_in targetAddr) override;
};