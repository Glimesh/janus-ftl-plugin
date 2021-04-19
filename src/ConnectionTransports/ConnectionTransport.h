/**
 * @file ConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "../Utilities/Result.h"

#include <chrono>
#include <functional>
#include <netinet/in.h>
#include <optional>
#include <span>
#include <vector>

/**
 * @brief
 *  A generic thread-safe network transport abstraction, allowing bytes to be read/written
 *  to a connection via a common interface.
 * 
 */
class ConnectionTransport
{
public:
    virtual ~ConnectionTransport() = default;

    /**
     * @brief Gets the IPv4 address of this connection, if it has one
     */
    virtual std::optional<sockaddr_in> GetAddr() = 0;

    /**
     * @brief Gets the IPv6 address of this connection, if it has one
     */
    virtual std::optional<sockaddr_in6> GetAddr6() = 0;

    /**
     * @brief
     *  Shuts down the connection.
     *  This function should block until the underlying transport/socket has been closed.
     */
    virtual void Stop() = 0;

    /**
     * @brief
        Read a set of bytes from the transport into the given buffer.
        Will timeout if there is nothing to read and return zero bytes.
     */
    virtual Result<ssize_t> Read(std::vector<std::byte>& buffer, std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Write a set of bytes to the transport
     */
    virtual Result<void> Write(const std::span<std::byte>& bytes) = 0;
};