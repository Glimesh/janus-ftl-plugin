/**
 * @file IngestServer.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "IngestConnection.h"
#include "ServiceConnection.h"
#include <sys/socket.h>
#include <vector>
#include <thread>
#include <memory>
#include <map>
#include <functional>

/**
 * @brief This class listens for incoming FTL ingest connections.
 */
class IngestServer
{
public:
    /* Constructor/Destructor */
    IngestServer(
        std::shared_ptr<ServiceConnection> serviceConnection,
        int listenPort = 8084,
        int socketQueueLimit = SOMAXCONN);

    /* Public methods */
    void Start();
    void Stop();
    // Callbacks
    void SetOnRequestMediaConnection(std::function<uint16_t (std::shared_ptr<IngestConnection>)> callback);

private:
    /* Private members */
    std::shared_ptr<ServiceConnection> serviceConnection;
    int listenPort;
    int socketQueueLimit;
    int listenSocketHandle;
    std::thread listenThread;
    // Stores connections that we don't yet have authentication information for
    std::vector<std::shared_ptr<IngestConnection>> pendingConnections;
    std::function<uint16_t (std::shared_ptr<IngestConnection>)> onRequestMediaConnection;

    /* Private methods */
    void startListenThread();
    void removeConnection(IngestConnection& connection);
    void connectionClosed(IngestConnection& connection);
    uint16_t mediaConnectionRequested(IngestConnection& connection);
};