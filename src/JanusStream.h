/**
 * @file JanusStream.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-03-10
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#pragma once

#include "FtlClient.h"
#include "JanusSession.h"
#include "RtpPacketSink.h"
#include "Rtp/RtpPacket.h"
#include "Utilities/FtlTypes.h"

#include <unordered_set>
#include <vector>

class JanusStream : public RtpPacketSink
{
public:
    /* Constructor/Destructor */
    JanusStream(
        ftl_channel_id_t channelId,
        ftl_stream_id_t streamId,
        MediaMetadata mediaMetadata);

    /* Public methods */
    void SendRtpPacket(const RtpPacket& packet) override;

    // Session methods
    void AddViewerSession(JanusSession* session);
    size_t RemoveViewerSession(JanusSession* session);
    std::unordered_set<JanusSession*> RemoveAllViewerSessions();
    size_t GetViewerCount() const;

    // Relay client methods
    void AddRelayClient(const std::string targetHostname, std::unique_ptr<FtlClient> client);
    size_t StopRelay(const std::string& targetHostname);
    void StopRelays();

    /* Getters/Setters */
    ftl_channel_id_t GetChannelId() const;
    ftl_stream_id_t GetStreamId() const;
    MediaMetadata GetMetadata() const;

private:
    struct Relay
    {
        std::string TargetHostname;
        std::unique_ptr<FtlClient> Client;
    };

    ftl_channel_id_t channelId;
    ftl_stream_id_t streamId;
    MediaMetadata mediaMetadata;
    std::unordered_set<JanusSession*> viewerSessions;
    std::list<Relay> relays;
    mutable std::mutex mutex;
};
