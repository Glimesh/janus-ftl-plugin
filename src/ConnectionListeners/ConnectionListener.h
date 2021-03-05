/**
 * @file ConnectionListener.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-03
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include <future>
#include <memory>

// Forward declarations
class ConnectionTransport;

/**
 * @brief The ConnectionListener interface defines an object that listens for incoming connections
 * and generates ConnectionTransports from them.
 * 
 */
class ConnectionListener
{
public:
    virtual ~ConnectionListener() = default;

    /**
     * @brief Starts listening for incoming connections, blocking the current thread
     * @param readyPromise a promise that is fulfilled as soon as the service is ready to accept
     * new connections
     */
    virtual void Listen(std::promise<void>&& readyPromise = std::promise<void>()) = 0;

    /**
     * @brief Stops listening for incoming connections, unblocking the Listen call
     */
    virtual void StopListening() = 0;

    /**
     * @brief Sets the callback that will be fired when a new connection transport 
     * has been established
     */
    virtual void SetOnNewConnection(
        std::function<void(ConnectionTransport*)> onNewConnection) = 0;
};