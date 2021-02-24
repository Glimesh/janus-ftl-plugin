/**
 * @file TcpConnectionListener.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-03
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include "TcpConnectionListener.h"

#include "../ConnectionTransports/NetworkSocketConnectionTransport.h"
#include "../Utilities/Util.h"

// #include <fmt/core.h>
#include <netinet/in.h>
// #include <spdlog/spdlog.h>
// #include <stdexcept>

#pragma region Constructor/Destructor
TcpConnectionListener::TcpConnectionListener(
    const int listenPort,
    const int socketQueueLimit) :
    listenPort(listenPort),
    socketQueueLimit(socketQueueLimit)
{ }
#pragma endregion Constructor/Destructor

#pragma region ConnectionTransport implementation
void TcpConnectionListener::Listen(std::promise<void>&& readyPromise)
{
    // TODO IPv6 binding, configurable binding interfaces...
    sockaddr_in socketAddress = { 0 };
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port = htons(listenPort);

    listenSocketHandle = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocketHandle < 0)
    {
        int error = errno;
        throw std::runtime_error(
            fmt::format(
                "Unable to create listen socket! Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    // Allow re-use so we don't get hung up trying to rebind
    int reUseOption = 1;
    if (setsockopt(
        listenSocketHandle,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reUseOption,
        sizeof(reUseOption)) != 0)
    {
        int error = errno;
        throw std::runtime_error(
            fmt::format(
                "Unable to set SO_REUSEADDR on listen socket! Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    int bindResult = bind(
        listenSocketHandle,
        (const sockaddr*)&socketAddress,
        sizeof(socketAddress));
    switch (bindResult)
    {
    case 0:
        break;
    case EADDRINUSE:
        throw std::runtime_error("FTL ingest could not bind to socket, "
            "this address is already in use.");
    case EACCES:
        throw std::runtime_error("FTL ingest could not bind to socket, "
            "access was denied.");
    default:
        throw std::runtime_error("FTL ingest could not bind to socket.");
    }
    
    int listenResult = listen(listenSocketHandle, socketQueueLimit);
    switch (listenResult)
    {
    case 0:
        break;
    case EADDRINUSE:
        throw std::runtime_error("FTL ingest could not listen on socket, "
            "this port is already in use.");
    default:
        throw std::runtime_error("FTL ingest could not listen on socket.");
    }

    // Now we begin listening
    readyPromise.set_value();
    while (true)
    {
        // Accept incoming connections
        int connectionHandle = accept(listenSocketHandle, nullptr, nullptr);
        if (connectionHandle == -1)
        {
            if (errno == EINVAL)
            {
                break;
            }
        }
        else
        {
            sockaddr_in acceptAddress = { 0 };
            socklen_t acceptLen = sizeof(acceptAddress);
            getpeername(connectionHandle, reinterpret_cast<sockaddr*>(&acceptAddress), &acceptLen);
            // Create a ConnectionTransport for this new connection
            auto transport = std::make_unique<NetworkSocketConnectionTransport>(
                NetworkSocketConnectionKind::Tcp,
                connectionHandle,
                acceptAddress);
            if (onNewConnection)
            {
                onNewConnection(std::move(transport));
            }
            else
            {
                // Warn that we've accepted a connection but nobody was listening!
            }
        }
    }
}

void TcpConnectionListener::StopListening()
{

}

void TcpConnectionListener::SetOnNewConnection(
    std::function<void(std::unique_ptr<ConnectionTransport>)> onNewConnection)
{
    this->onNewConnection = onNewConnection;
}
#pragma endregion ConnectionTransport implementation