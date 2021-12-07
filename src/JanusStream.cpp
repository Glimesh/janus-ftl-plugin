/**
 * @file JanusStream.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-03-10
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include "JanusStream.h"

#pragma region Constructor/Destructor
JanusStream::JanusStream(
    ftl_channel_id_t channelId,
    ftl_stream_id_t streamId,
    MediaMetadata mediaMetadata) :
    channelId(channelId),
    streamId(streamId),
    mediaMetadata(mediaMetadata)
{ }
#pragma endregion

#pragma region Public methods
void JanusStream::SendRtpPacket(const RtpPacket& packet)
{
    std::lock_guard lock(mutex);

    for (const auto& session : viewerSessions)
    {
        session->SendRtpPacket(packet, mediaMetadata);
    }

    for (const auto& relay : relays)
    {
        relay.Client->RelayPacket(packet);
    }
}

void JanusStream::AddViewerSession(JanusSession* session)
{
    std::lock_guard lock(mutex);
    viewerSessions.insert(session);
}

size_t JanusStream::RemoveViewerSession(JanusSession* session)
{
    std::lock_guard lock(mutex);
    return viewerSessions.erase(session);
}

std::unordered_set<JanusSession*> JanusStream::RemoveAllViewerSessions()
{
    std::lock_guard lock(mutex);
    std::unordered_set<JanusSession*> removedSessions;
    viewerSessions.swap(removedSessions);
    return removedSessions;
}

size_t JanusStream::GetViewerCount() const
{
    std::lock_guard lock(mutex);
    return viewerSessions.size();
}

void JanusStream::AddRelayClient(const std::string targetHostname,
    std::unique_ptr<FtlClient> client)
{
    std::lock_guard lock(mutex);
    relays.push_back(Relay { .TargetHostname = targetHostname, .Client = std::move(client) });
}

size_t JanusStream::StopRelay(const std::string& targetHostname)
{
    std::list<Relay> removedRelays;
    {
        std::lock_guard lock(mutex);
        for (auto it = relays.begin();
            it != relays.end();)
        {
            if (it->TargetHostname == targetHostname)
            {
                removedRelays.push_back(std::move(*it));
                it = relays.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    for (Relay& relay : removedRelays)
    {
        spdlog::info("Stopping relay for channel {} / stream {} -> {}...",
            channelId, streamId, relay.TargetHostname);
         relay.Client->Stop();
    }
    return removedRelays.size();
}

void JanusStream::StopRelays()
{
    std::list<Relay> removedRelays;
    {
        std::lock_guard lock(mutex);
        relays.swap(removedRelays);
    }
    for (const auto& relay : removedRelays)
    {
        spdlog::info("Stopping relay for channel {} / stream {} -> {}...",
            channelId, streamId, relay.TargetHostname);
        relay.Client->Stop();
    }
}

#pragma endregion

#pragma region Getters/setters

ftl_channel_id_t JanusStream::GetChannelId() const
{
    return channelId;
}
ftl_stream_id_t JanusStream::GetStreamId() const
{
    return streamId;
}
MediaMetadata JanusStream::GetMetadata() const
{
    return mediaMetadata;
}

#pragma endregion
