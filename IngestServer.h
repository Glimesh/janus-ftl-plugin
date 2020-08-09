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
#include <sys/socket.h>
#include <vector>
#include <thread>

/**
 * @brief This class listens for incoming FTL ingest connections.
 */
class IngestServer
{
public:
    /* Constructor/Destructor */
    IngestServer(int listenPort = 8084, int socketQueueLimit = SOMAXCONN);

    /* Public methods */
    void Start();
    void Stop();

private:
    /* Private members */
    int listenPort;
    int socketQueueLimit;
    int listenSocketHandle;
    std::thread listenThread;
    std::vector<std::shared_ptr<IngestConnection>> pendingConnections;

    /* Private methods */
    void startListenThread();
};