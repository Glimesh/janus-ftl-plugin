/**
 * @file IngestServer.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "IngestServer.h"
extern "C"
{
    #include <debug.h>
}
#include <netinet/in.h>
#include <cerrno>
#include <stdexcept>
#include <memory>
#include <unistd.h>

#pragma region Constructor/Destructor
IngestServer::IngestServer(
    std::shared_ptr<ServiceConnection> serviceConnection,
    int listenPort,
    int socketQueueLimit) : 
    serviceConnection(serviceConnection),
    listenPort(listenPort),
    socketQueueLimit(socketQueueLimit)
{ }
#pragma endregion

#pragma region Public methods
void IngestServer::Start()
{
    sockaddr_in socketAddress = { 0 };
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port = htons(listenPort);

    listenSocketHandle = socket(AF_INET, SOCK_STREAM, 0);
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

    listenThread = std::thread(&IngestServer::startListenThread, this);
}

void IngestServer::Stop()
{
    for (const auto& ingestConnection : pendingConnections)
    {
        ingestConnection->Stop();
    }
    pendingConnections.clear();
    shutdown(listenSocketHandle, SHUT_RDWR);
    listenThread.join();
}

void IngestServer::SetOnRequestMediaConnection(
    std::function<uint16_t (std::shared_ptr<IngestConnection>)> callback)
{
    onRequestMediaConnection = callback;
}
#pragma endregion

#pragma region Private methods
void IngestServer::startListenThread()
{
    JANUS_LOG(LOG_INFO, "FTL: Ingest server is listening on port %d\n", listenPort);
    while (true)
    {
        // Accept incoming connections, manage them as "pending" until the
        // FTL handshake is completed.
        
        int connectionHandle = accept(listenSocketHandle, nullptr, nullptr);
        if (connectionHandle == -1)
        {
            if (errno == EINVAL)
            {
                JANUS_LOG(LOG_INFO, "FTL: Ingest server is being shut down.\n");
                break;
            }
        }
        else
        {
            sockaddr_in acceptAddress = { 0 };
            socklen_t acceptLen = sizeof(acceptAddress);
            getpeername(connectionHandle, reinterpret_cast<sockaddr*>(&acceptAddress), &acceptLen);
            JANUS_LOG(LOG_INFO, "FTL: Ingest server accepted connection...\n");
            std::shared_ptr<IngestConnection> connection = 
                std::make_shared<IngestConnection>(
                    connectionHandle,
                    acceptAddress,
                    serviceConnection);
            pendingConnections.push_back(connection);
                
            connection->SetOnClosed(std::bind(
                &IngestServer::connectionClosed,
                this,
                std::placeholders::_1));
            connection->SetOnRequestMediaConnection(std::bind(
                &IngestServer::mediaConnectionRequested,
                this,
                std::placeholders::_1));
            connection->Start();
        }
    }
}

void IngestServer::removeConnection(IngestConnection& connection)
{
    for (auto it = pendingConnections.begin(); it != pendingConnections.end(); ++it)
    {
        const auto& pendingConnection = *it;
        if (pendingConnection.get() == &connection)
        {
            pendingConnections.erase(it);
            break;
        }
    }
}

void IngestServer::connectionClosed(IngestConnection& connection)
{
    // Stop and remove this connection
    connection.Stop();
    removeConnection(connection);

    uint32_t connectionCount = pendingConnections.size();

    JANUS_LOG(LOG_INFO, "FTL: Pending ingest connection closed. %d pending ingest connections.\n",
        connectionCount);
}

uint16_t IngestServer::mediaConnectionRequested(IngestConnection& connection)
{
    // Find our reference to this connection and remove it
    std::shared_ptr<IngestConnection> connectionReference;
    for (auto it = pendingConnections.begin(); it != pendingConnections.end(); ++it)
    {
        std::shared_ptr<IngestConnection>& c = *it;
        if (c.get() == &connection)
        {
            connectionReference = *it;
            pendingConnections.erase(it);
            break;
        }
    }

    if (connectionReference == nullptr)
    {
        throw std::runtime_error("Could not find reference to connection.");
    }

    // Pass this connection out to callback
    if (onRequestMediaConnection != nullptr)
    {
        // Relinquish callbacks
        connectionReference->SetOnClosed(nullptr);
        connectionReference->SetOnRequestMediaConnection(nullptr);
        return onRequestMediaConnection(connectionReference);
    }
    else
    {
        throw std::runtime_error("Callback from IngestServer to request new media connection failed.");
    }
}
#pragma endregion