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
#include "CredStore.h"
#include <sys/socket.h>
#include <vector>
#include <thread>
#include <memory>
#include <map>

/**
 * @brief This class listens for incoming FTL ingest connections.
 */
class IngestServer
{
public:
    /* Constructor/Destructor */
    IngestServer(
        std::shared_ptr<CredStore> credStore,
        int listenPort = 8084,
        int socketQueueLimit = SOMAXCONN);

    /* Public methods */
    void Start();
    void Stop();

private:
    /* Private members */
    std::shared_ptr<CredStore> credStore;
    int listenPort;
    int socketQueueLimit;
    int listenSocketHandle;
    std::thread listenThread;
    std::vector<std::shared_ptr<IngestConnection>> pendingConnections;
    std::map<uint32_t, std::shared_ptr<IngestConnection>> authenticatedConnections;

    /* Private methods */
    void startListenThread();
    void removeConnection(IngestConnection& connection);
    void connectionStateChanged(IngestConnection& connection, IngestConnectionState newState);
    uint16_t mediaPortRequested(IngestConnection& connection);
};