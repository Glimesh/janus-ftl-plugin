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
    std::shared_ptr<CredStore> credStore,
    int listenPort,
    int socketQueueLimit) : 
    credStore(credStore),
    listenPort(listenPort),
    socketQueueLimit(socketQueueLimit)
{ }
#pragma endregion

#pragma region Public methods
void IngestServer::Start()
{
    struct sockaddr_in socketAddress;
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
    //listenThread.detach();
}

void IngestServer::Stop()
{
    stopping = true;
    // TODO: kill listen thread
    // https://stackoverflow.com/questions/44259468/how-to-interrupt-accept-in-a-tcp-ip-server
    for (const auto& ingestConnection : pendingConnections)
    {
        ingestConnection->Stop();
    }
    pendingConnections.clear();
    shutdown(listenSocketHandle, SHUT_RDWR);
    //close(listenSocketHandle);
    JANUS_LOG(LOG_INFO, "FTL: Joining server thread\n");
    listenThread.join();
    JANUS_LOG(LOG_INFO, "FTL: Server thread joined\n");
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
            JANUS_LOG(LOG_INFO, "FTL: Ingest server accepted connection...\n");
            std::shared_ptr<IngestConnection> connection = 
                std::make_shared<IngestConnection>(connectionHandle, credStore);
            pendingConnections.push_back(connection);
            connection->Start();
        }
    }
    JANUS_LOG(LOG_INFO, "FTL: Ingest server listen thread terminating\n");
}
#pragma endregion