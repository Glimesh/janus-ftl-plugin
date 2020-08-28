/**
 * @file RelayThreadPool.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-28
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "RelayThreadPool.h"
#include "FtlStreamStore.h"
#include "FtlStream.h"

extern "C"
{
    #include <debug.h>
}

#pragma region Constructor/Destructor
RelayThreadPool::RelayThreadPool(
    std::shared_ptr<FtlStreamStore> ftlStreamStore,
    unsigned int threadCount) : 
    ftlStreamStore(ftlStreamStore),
    threadCount(threadCount)
{ }
#pragma endregion

#pragma region Public methods
void RelayThreadPool::Start()
{
    std::lock_guard<std::mutex> lock(threadVectorMutex);
    for (unsigned int i = 0; i < threadCount; ++i)
    {
        auto thread = std::thread(&RelayThreadPool::relayThreadMethod, this, i);
        thread.detach();
        relayThreads[i] = std::move(thread);
    }
}

void RelayThreadPool::Stop()
{
    SetThreadCount(0);

    // TODO: Wait for threads to exit
}

void RelayThreadPool::RelayPacket(RtpRelayPacket packet)
{
    {
        std::lock_guard<std::mutex> lock(relayMutex);
        packetRelayQueue.push(packet);
    }
    relayThreadCondition.notify_one();
}

void RelayThreadPool::SetThreadCount(unsigned int newThreadCount)
{
    std::lock_guard<std::mutex> lock(threadVectorMutex);
    if (isStarted && (newThreadCount > threadCount))
    {
        for (unsigned int i = threadCount; i < newThreadCount; ++i)
        {
            auto thread = std::thread(&RelayThreadPool::relayThreadMethod, this, i);
            thread.detach();
            relayThreads[i] = std::move(thread);
        }
    }
    threadCount = newThreadCount;
}
#pragma endregion

#pragma region Private methods
void RelayThreadPool::relayThreadMethod(unsigned int threadNumber)
{
    JANUS_LOG(LOG_INFO, "FTL: Relay thread pool thread #%d started.\n", threadNumber);
    std::unique_lock<std::mutex> lock(relayMutex);

    while (true)
    {
        // Wait to be signaled that we have new packets to process.
        // NOTE: `wait` will automatically release the lock until the condition
        // variable is triggered, at which point it will hold the lock again.
        relayThreadCondition.wait(lock,
            [this, threadNumber]()
            {
                return (packetRelayQueue.size() > 0) || (threadCount < threadNumber);
            });

        // If we're stopping, release the lock and exit the thread.
        if (threadCount < threadNumber)
        {
            break;
        }

        // Pop a packet off of the queue, then clear and unlock the mutex so it can continue
        // being filled while we're processing.
        RtpRelayPacket packet = std::move(packetRelayQueue.front());
        packetRelayQueue.pop();
        lock.unlock();

        // Find the stream we're sending this to the viewers of
        std::shared_ptr<FtlStream> originStream = 
            ftlStreamStore->GetStreamByChannelId(packet.channelId);

        if (originStream == nullptr)
        {
            JANUS_LOG(
                LOG_WARN,
                "FTL: Packet relay failed for non-existant stream with channel ID %lu\n",
                packet.channelId);
            continue;
        }

        std::list<std::shared_ptr<JanusSession>> channelSessions = 
            originStream->GetViewers();
        for (const auto& session : channelSessions)
        {
            session->SendRtpPacket(packet);
        }

        lock.lock();
    }

    // Remove from thread pool
    {
        std::lock_guard<std::mutex> threadPoolLock(threadVectorMutex);
        relayThreads.erase(threadNumber);
    }
    JANUS_LOG(LOG_INFO, "FTL: Relay thread pool thread #%d terminated.\n", threadNumber);
}
#pragma endregion