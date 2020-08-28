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

#include "RtpRelayPacket.h"

#include <memory>
#include <mutex>
#include <map>
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
        unsigned int threadCount = DEFAULT_THREAD_COUNT);

    /* Public methods */
    void Start();
    void Stop();
    void RelayPacket(RtpRelayPacket packet);
    void SetThreadCount(unsigned int newThreadCount);
private:
    /* Private members */
    static const unsigned int DEFAULT_THREAD_COUNT = 5;
    const std::shared_ptr<FtlStreamStore> ftlStreamStore;
    bool isStarted = false;
    unsigned int threadCount;
    // Packet relay threads
    std::mutex threadVectorMutex;
    std::mutex relayMutex;
    std::map<unsigned int, std::thread> relayThreads;
    std::condition_variable relayThreadCondition;
    std::queue<RtpRelayPacket> packetRelayQueue;

    /* Private methods */
    void relayThreadMethod(unsigned int threadNumber);
};
