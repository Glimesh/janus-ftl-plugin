/**
 * @file TcpConnectionListener.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-03
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "ConnectionListener.h"

#include <sys/socket.h>

/**
 * @brief The TcpConnectionListener listens for incoming TCP connections and outputs
 * a TcpSocketConnectionTransport instance for each new connection.
 * 
 */
class TcpConnectionListener : public ConnectionListener
{
public:
    /* Constructor/Destructor */
    TcpConnectionListener(
        const int listenPort,
        const int socketQueueLimit = SOMAXCONN);

    /* ConnectionTransport implementation */
    void Listen(std::promise<void>&& readyPromise = std::promise<void>()) override;
    void StopListening() override;
    void SetOnNewConnection(
        std::function<void(ConnectionTransport*)> onNewConnection) override;

private:
    const int listenPort;
    const int socketQueueLimit;
    int listenSocketHandle = 0;
    std::function<void(ConnectionTransport*)> onNewConnection;
};