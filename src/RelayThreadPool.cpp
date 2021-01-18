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

#include "FtlClient.h"
#include "FtlStreamStore.h"
#include "FtlStream.h"
#include "JanusSession.h"

extern "C"
{
    #include <debug.h>
}

#pragma region Constructor/Destructor
RelayThreadPool::RelayThreadPool(
    std::shared_ptr<FtlStreamStore> ftlStreamStore) : 
    ftlStreamStore(ftlStreamStore)
{ }
#pragma endregion

#pragma region Public methods
void RelayThreadPool::Start()
{
    std::lock_guard<std::mutex> lock(threadVectorMutex);

    // TODO: A thread pool with just one thread is not a pool, consider ripping
    // this code out entirely or reworking it if we do need a relay pool to 
    // send the relayed packets in the order they were queued.
    // https://github.com/Glimesh/janus-ftl-plugin/issues/57
    auto thread = std::thread(&RelayThreadPool::relayThreadMethod, this);
    relayThreads.push_back(std::move(thread));
}

void RelayThreadPool::Stop()
{
    std::lock_guard<std::mutex> lock(threadVectorMutex);

    // Notify threads that we're stopping
    stopping = true;
    relayThreadCondition.notify_all();

    // Join all threads
    for (auto& thread : relayThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    // Clear thread pool
    relayThreads.clear();
}

void RelayThreadPool::RelayPacket(RtpRelayPacket packet)
{
    {
        std::lock_guard<std::mutex> lock(relayMutex);
        packetRelayQueue.push(packet);
    }
    relayThreadCondition.notify_one();
}
#pragma endregion

#pragma region Private methods
void RelayThreadPool::relayThreadMethod()
{
    JANUS_LOG(LOG_INFO, "FTL: Relay thread started.\n");
    std::unique_lock<std::mutex> lock(relayMutex);

    while (true)
    {
        // Wait to be signaled that we have new packets to process.
        // NOTE: `wait` will automatically release the lock until the condition
        // variable is triggered, at which point it will hold the lock again.
        relayThreadCondition.wait(lock,
            [this]()
            {
                return (packetRelayQueue.size() > 0) || stopping;
            });

        // If we're stopping, release the lock and exit the thread.
        if (stopping)
        {
            break;
        }

        // Pop a packet off of the queue, then clear and unlock the mutex so it can continue
        // being filled while we're processing.
        RtpRelayPacket packet = std::move(packetRelayQueue.front());
        packetRelayQueue.pop();
        lock.unlock();

        // Are we relaying this packet to anyone?
        const std::list<FtlStreamStore::RelayStore> relays =
            ftlStreamStore->GetRelaysForChannelId(packet.channelId);
        for (const auto& relay : relays)
        {
            relay.FtlClientInstance->RelayPacket(packet);
        }

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

    JANUS_LOG(LOG_INFO, "FTL: Relay thread terminated.\n");
}
#pragma endregion