/**
 * @file RelayThreadPool.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-28
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "Utilities/FtlTypes.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>

class FtlStreamStore;
class FtlStream;

class RelayThreadPool
{
public:
    /* Constructor/Destructor */
    RelayThreadPool(
        std::shared_ptr<FtlStreamStore> ftlStreamStore,
        unsigned int threadCount = std::thread::hardware_concurrency());

    /* Public methods */
    void Start();
    void Stop();
    void RelayPacket(RtpRelayPacket packet);

private:
    /* Private members */
    const std::shared_ptr<FtlStreamStore> ftlStreamStore;
    unsigned int threadCount;
    std::atomic<bool> stopping { false };
    // Packet relay threads
    std::mutex threadVectorMutex;
    std::mutex relayMutex;
    std::vector<std::thread> relayThreads;
    std::condition_variable relayThreadCondition;
    std::queue<RtpRelayPacket> packetRelayQueue;

    /* Private methods */
    void relayThreadMethod(unsigned int threadNumber);
};
