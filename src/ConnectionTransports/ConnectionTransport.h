/**
 * @file ConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "Utilities/Result.h"

#include <functional>
#include <netinet/in.h>
#include <optional>
#include <vector>

/**
 * @brief
 *  A generic network transport abstraction, allowing bytes to be read/written
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
     *  Starts the connection transport in a new thread.
     *  This function should block until the transport is ready to receive bytes.
     */
    virtual Result<void> StartAsync() = 0;

    /**
     * @brief
     *  Shuts down the connection.
     *  This function should block until the underlying transport/socket has been closed.
     */
    virtual void Stop() = 0;

    /**
     * @brief Write a set of bytes to the transport
     */
    virtual void Write(const std::vector<std::byte>& bytes) = 0;

    /**
     * @brief Sets the callback that will fire when this connection has been closed.
     */
    virtual void SetOnConnectionClosed(std::function<void(void)> onConnectionClosed) = 0;

    /**
     * @brief Sets the callback that will fire when this connection has received incoming data.
     */
    virtual void SetOnBytesReceived(
        std::function<void(const std::vector<std::byte>&)> onBytesReceived) = 0;
};