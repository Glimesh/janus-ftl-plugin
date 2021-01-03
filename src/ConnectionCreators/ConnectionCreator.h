/**
 * @file ConnectionCreator.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-03
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include <memory>
#include <netinet/in.h>
#include <optional>

// Forward declarations
class ConnectionTransport;

/**
 * @brief The ConnectionCreator is a factory-pattern style class used to create stateless
 * connections
 */
class ConnectionCreator
{
public:
    ~ConnectionCreator() = default;

    /**
     * @brief Create a ConnectionTransport targeting the provided port and IPv4/v6 addresses.
     */
    virtual std::unique_ptr<ConnectionTransport> CreateConnection(
        int port,
        std::optional<sockaddr_in> targetAddr,
        std::optional<sockaddr_in6> targetAddr6) = 0;
};