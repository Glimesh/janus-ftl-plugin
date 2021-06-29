/**
 * @file FtlStream.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */
#pragma once

#include "FtlMediaConnection.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <set>
#include <vector>

class ConnectionTransport;
class FtlControlConnection;

/**
 * @brief Manages the FTL media stream, accepting incoming RTP packets.
 */
class FtlStream
{
public:
    /* Public types */
    using ClosedCallback = std::function<void(FtlStream*)>;

    /* Constructor/Destructor */
    FtlStream(
        std::shared_ptr<FtlControlConnection> controlConnection,
        const ftl_stream_id_t streamId,
        const ClosedCallback onClosed,
        const uint32_t rollingSizeAvgMs,
        const bool nackLostPackets);

    /* Public methods */
    Result<void> StartMediaConnection(
        std::unique_ptr<ConnectionTransport> mediaTransport,
        const uint16_t mediaPort,
        const MediaMetadata mediaMetadata,
        const FtlMediaConnection::RtpPacketCallback onRtpPacket
    );
    void RequestStop();
    void ControlConnectionStopped(FtlControlConnection* controlConnection);

    /* Getters/Setters */
    ftl_channel_id_t GetChannelId() const;
    ftl_stream_id_t GetStreamId() const;
    Result<FtlStreamStats> GetStats();
    Result<FtlKeyframe> GetKeyframe();

private:
    /* Private members */
    std::unique_ptr<FtlMediaConnection> mediaConnection;
    const std::shared_ptr<FtlControlConnection> controlConnection;
    const ftl_stream_id_t streamId;
    const ClosedCallback onClosed;
    const uint32_t rollingSizeAvgMs;
    const bool nackLostPackets;
    bool closed = false;
    std::mutex mutex;

    /* Private methods */
    void onControlConnectionClosed();
    void onMediaConnectionClosed();
};