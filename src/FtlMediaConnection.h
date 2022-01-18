/**
 * @file FtlMediaConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */
#pragma once

#include "Rtp/RtpPacket.h"
#include "Rtp/SequenceTracker.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <set>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class ConnectionTransport;
class FtlControlConnection;

/**
 * @brief Manages the FTL media stream, accepting incoming RTP packets.
 */
class FtlMediaConnection
{
public:
    /* Public types */
    using ClosedCallback = std::function<void(FtlMediaConnection&)>;
    using RtpPacketCallback = std::function<void(const RtpPacket&)>;

    /* Constructor/Destructor */
    FtlMediaConnection(
        std::unique_ptr<ConnectionTransport> transport,
        const MediaMetadata mediaMetadata,
        const ftl_channel_id_t channelId,
        const ftl_stream_id_t streamId,
        const ClosedCallback onClosed,
        const RtpPacketCallback onRtpPacket,
        const uint32_t rollingSizeAvgMs = 2000,
        const bool nackLostPackets = true);

    /* Public methods */
    void RequestStop();

    /* Getters/Setters */
    FtlStreamStats GetStats();
    Result<FtlKeyframe> GetKeyframe();

private:
    /* Private types */
    struct Frame
    {
        std::list<RtpPacket> Packets;
        rtp_timestamp_t Timestamp;
        
        bool IsComplete() const;
        void InsertPacketInSequenceOrder(const RtpPacket &rtpPacket);
    };
    struct SsrcData
    {
        uint32_t PacketsReceived = 0;
        std::map<std::chrono::time_point<std::chrono::steady_clock>, uint16_t>
            RollingBytesReceivedByTime;
        Frame CurrentKeyframe;
        Frame PendingKeyframe;
        SequenceTracker Tracker;
    };

    /* Constants */
    static constexpr rtp_payload_type_t FTL_PAYLOAD_TYPE_SENDER_REPORT = 200;
    static constexpr rtp_payload_type_t FTL_PAYLOAD_TYPE_PING          = 250;
    static constexpr std::chrono::milliseconds READ_TIMEOUT{200};

    /* Private members */
    const std::unique_ptr<ConnectionTransport> transport;
    const MediaMetadata mediaMetadata;
    const ftl_channel_id_t channelId;
    const ftl_stream_id_t streamId;
    const ClosedCallback onClosed;
    const RtpPacketCallback onRtpPacket;
    const uint32_t rollingSizeAvgMs;
    const bool nackLostPackets;
    // Stream data
    std::shared_mutex dataMutex;
    time_t startTime { 0 };
    std::chrono::time_point<std::chrono::steady_clock> steadyStartTime;
    std::unordered_map<rtp_ssrc_t, SsrcData> ssrcData;
    // Thread to read and process packets from the connection, must be initialized last
    std::jthread thread;

    /* Private methods */
    void threadBody(std::stop_token stopToken);
    void onBytesReceived(const std::vector<std::byte> &bytes);

    // Packet handling
    void handleRtpPacket(const std::vector<std::byte> &packetBytes);
    void handleMediaPacket(const std::vector<std::byte> &packetBytes);
    void handlePing(const std::vector<std::byte> &packetBytes);
    void handleSenderReport(const std::vector<std::byte> &packetBytes);

    // Helpers for handling media packets
    void updateMediaPacketStats(
        const RtpPacket &rtpPacket,
        SsrcData &data,
        const std::unique_lock<std::shared_mutex>& dataLock);
    void processNacks(
        const RtpPacket &packet,
        SsrcData &data,
        const std::unique_lock<std::shared_mutex>& dataLock);
    void captureVideoKeyframe(
        const RtpPacket &rtpPacket,
        SsrcData &data,
        const std::unique_lock<std::shared_mutex>& dataLock);
    void captureH264VideoKeyframe(
        const RtpPacket &rtpPacket,
        SsrcData &data,
        const std::unique_lock<std::shared_mutex>& dataLock);
    void sendQueuedNacks(
        const rtp_ssrc_t ssrc,
        SsrcData &data,
        const std::unique_lock<std::shared_mutex>& dataLock);
    void updateNackQueue(
        const rtp_extended_sequence_num_t extendedSeqNum,
        const std::set<rtp_extended_sequence_num_t> &missingSequences,
        SsrcData &data,
        const std::unique_lock<std::shared_mutex>& dataLock);
    void sendNack(
        const rtp_ssrc_t ssrc,
        const rtp_extended_sequence_num_t seq,
        const uint16_t followingLostPacketsBitmask);
};
