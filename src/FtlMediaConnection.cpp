/**
 * @file FtlMediaConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "FtlMediaConnection.h"

#include "ConnectionTransports/NetworkSocketConnectionTransport.h"
#include "FtlControlConnection.h"
#include "Rtp/RtpPacket.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <poll.h>
#include <spdlog/fmt/bin_to_hex.h>

#pragma region Constructor / Destructor
FtlMediaConnection::FtlMediaConnection(
    std::unique_ptr<ConnectionTransport> transport,
    const MediaMetadata mediaMetadata,
    const ftl_channel_id_t channelId,
    const ftl_stream_id_t streamId,
    const ClosedCallback onClosed,
    const RtpPacketCallback onRtpPacketBytes,
    const uint32_t rollingSizeAvgMs,
    const bool nackLostPackets)
    : transport(std::move(transport)),
      mediaMetadata(mediaMetadata),
      channelId(channelId),
      streamId(streamId),
      onClosed(onClosed),
      onRtpPacketBytes(onRtpPacketBytes),
      rollingSizeAvgMs(rollingSizeAvgMs),
      nackLostPackets(nackLostPackets),
      thread(std::jthread(std::bind(&FtlMediaConnection::threadBody, this, std::placeholders::_1)))
{
    // Prepare stream data stores to accept packets from SSRCs specified by control handshake
    ssrcData.try_emplace(mediaMetadata.AudioSsrc);
    ssrcData.try_emplace(mediaMetadata.VideoSsrc);

    // Record start time
    startTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    steadyStartTime = std::chrono::steady_clock::now();
    spdlog::info("Media stream receiving for Channel {} / Stream {}",
                 channelId, streamId);
}
#pragma endregion

#pragma region Public methods
void FtlMediaConnection::RequestStop()
{
    thread.request_stop();
}

FtlStreamStats FtlMediaConnection::GetStats()
{
    std::shared_lock lock(dataMutex);
    FtlStreamStats stats{0};
    uint32_t rollingBytesReceived = 0;
    stats.StartTime = startTime;
    stats.DurationSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - steadyStartTime)
                                .count();
    for (const auto &dataPair : ssrcData)
    {
        const SsrcData &data = dataPair.second;
        stats.PacketsReceived += data.PacketsReceived;
        stats.PacketsNacked += data.PacketsNacked;
        stats.PacketsLost += data.PacketsLost;
        for (const auto &bytesPair : dataPair.second.RollingBytesReceivedByTime)
        {
            rollingBytesReceived += bytesPair.second;
        }
    }
    stats.RollingAverageBitrateBps = (rollingBytesReceived * 8) / (rollingSizeAvgMs / 1000.0f);

    return stats;
}

Result<FtlKeyframe> FtlMediaConnection::GetKeyframe()
{
    std::shared_lock lock(dataMutex);
    // Return the last available keyframe for the default video ssrc
    if (ssrcData.count(mediaMetadata.VideoSsrc) <= 0)
    {
        return Result<FtlKeyframe>::Error(
            fmt::format("No ssrc data available for video ssrc {}", mediaMetadata.VideoSsrc));
    }

    std::list<RtpPacket> &currentKeyframePackets =
        ssrcData.at(mediaMetadata.VideoSsrc).CurrentKeyframePackets;

    FtlKeyframe keyframe{mediaMetadata.VideoCodec};
    std::transform(
        currentKeyframePackets.begin(),
        currentKeyframePackets.end(),
        std::back_inserter(keyframe.Packets),
        [](const RtpPacket &packet) -> std::vector<std::byte> { return packet.Bytes; });
    return Result<FtlKeyframe>::Success(keyframe);
}
#pragma endregion

#pragma region Private methods
void FtlMediaConnection::threadBody(std::stop_token stopToken)
{
    std::vector<std::byte> buffer;

    while (!stopToken.stop_requested())
    {
        auto result = transport->Read(buffer, READ_TIMEOUT);
        if (result.IsError)
        {
            spdlog::error("Failed to read from media connection transport: {}",
                          result.ErrorMessage);
            break;
        }

        if (result.Value > 0)
        {
            onBytesReceived(buffer);
        }
    }

    spdlog::debug("Stopping media connection thread for Channel {} / Stream {}",
                  channelId, streamId);
    transport->Stop();
    if (onClosed)
    {
        onClosed(*this);
    }
}

void FtlMediaConnection::onBytesReceived(const std::vector<std::byte> &bytes)
{
    if (bytes.size() < 12)
    {
        // This packet is too small to have an RTP header.
        spdlog::warn(
            "Channel {} / stream {} received non-RTP packet of size {} (< 12 bytes). Discarding...",
            channelId, streamId, bytes.size());
        return;
    }

    handleRtpPacket(bytes);
}

std::set<rtp_extended_sequence_num_t> FtlMediaConnection::insertPacketInSequenceOrder(
    std::list<RtpPacket> &packetList, const RtpPacket &packet)
{
    const auto seqNum = packet.ExtendedSequenceNum;
    std::set<rtp_extended_sequence_num_t> missingSequences;
    // If the list is empty, just add it and we're done.
    if (packetList.size() == 0)
    {
        packetList.push_back(packet);
        return missingSequences;
    }
    // Insert the packet into list according to sequence number
    bool wasInserted = false;
    for (auto it = packetList.rbegin(); it != packetList.rend(); it++)
    {
        const RtpPacket &cachedPacket = *it;
        const auto cachedSeqNum = cachedPacket.ExtendedSequenceNum;

        // Have we found our insertion point in the sorted packet list?
        if (seqNum > cachedSeqNum)
        {
            // If there are any gaps between the sequence numbers, mark them as missing.
            if (seqNum - 1 != cachedSeqNum)
            {
                for (rtp_extended_sequence_num_t missingSeqNum = (cachedSeqNum + 1);
                     (missingSeqNum < seqNum); missingSeqNum++)
                {
                    missingSequences.insert(missingSeqNum);
                }
            }
            packetList.insert(it.base(), packet);
            wasInserted = true;
            break;
        }
    }

    if (!wasInserted)
    {
        // The packet's sequence number was older than the whole list - insert at the very front.
        const auto firstSeqNum = packetList.front().ExtendedSequenceNum;

        // Mark in-between packets missing.
        for (rtp_extended_sequence_num_t missingSeqNum = (seqNum + 1);
             (missingSeqNum < firstSeqNum); missingSeqNum++)
        {
            missingSequences.insert(missingSeqNum);
        }
        packetList.push_front(packet);
    }

    return missingSequences;
}

void FtlMediaConnection::handleRtpPacket(const std::vector<std::byte> &packetBytes)
{
    const RtpHeader *rtpHeader = RtpPacket::GetRtpHeader(packetBytes);
    rtp_ssrc_t ssrc = ntohl(rtpHeader->Ssrc);

    // Handle audio/video packets
    if ((ssrc == mediaMetadata.AudioSsrc) ||
        (ssrc == mediaMetadata.VideoSsrc))
    {
        handleMediaPacket(packetBytes);
    }
    else
    {
        // FTL implementation uses the marker bit space for payload types above 127
        // when the payload type is not audio or video. So we need to reconstruct it.
        uint8_t payloadType =
            ((static_cast<uint8_t>(rtpHeader->MarkerBit) << 7) |
             static_cast<uint8_t>(rtpHeader->Type));

        if (payloadType == FTL_PAYLOAD_TYPE_PING)
        {
            handlePing(packetBytes);
        }
        else if (payloadType == FTL_PAYLOAD_TYPE_SENDER_REPORT)
        {
            handleSenderReport(packetBytes);
        }
        else
        {
            spdlog::warn("Unknown RTP payload type {} (orig {})\n", payloadType,
                         rtpHeader->Type);
        }
    }
}

void FtlMediaConnection::handleMediaPacket(const std::vector<std::byte> &packetBytes)
{
    const RtpHeader *rtpHeader = RtpPacket::GetRtpHeader(packetBytes);
    const rtp_ssrc_t ssrc = ntohl(rtpHeader->Ssrc);
    const rtp_sequence_num_t seqNum = ntohs(rtpHeader->SequenceNumber);

    if (ssrcData.count(ssrc) <= 0)
    {
        const rtp_payload_type_t payloadType = ntohs(rtpHeader->Type);
        spdlog::warn("Received RTP payload type {} with unexpected ssrc {}", payloadType, ssrc);
        return;
    }

    std::unique_lock lock(dataMutex);
    SsrcData &data = ssrcData.at(ssrc);

    // The FTL client will sometimes send a bunch of audio packets first as a 'speed test'.
    // We should ignore these until we see our first video packet show up.
    if ((ssrc == mediaMetadata.AudioSsrc) &&
        (ssrcData.count(mediaMetadata.VideoSsrc) > 0) &&
        (ssrcData.at(mediaMetadata.VideoSsrc).PacketsReceived <= 0))
    {
        return;
    }

    auto extendedSeq = data.NackQueue.Track(seqNum);

    // Keep the sending of NACKs behind a feature toggle for now
    // https://github.com/Glimesh/janus-ftl-plugin/issues/95
    if (nackLostPackets)
    {
        auto missing = data.NackQueue.GetMissing();

        for (auto it = missing.rbegin(); it != missing.rend();)
        {
            // TODO do better than cheat and just send one packet per NACK
            rtp_extended_sequence_num_t seq = *it;
            sendNack(ssrc, seq, 0);
            data.NackQueue.NackSent(seq);
            ++it;

            // // Group missing sequence numbers into NACK packets which use a 16-bit
            // // bitmask to potentialy nack multiple numbers with a single packet.
            // rtp_extended_sequence_num_t nackStartSeq = *it;

            // // Build bitmask of current and up to 15 following missing sequence numbers
            // uint16_t bitmask = 0;
            // while (it != missing.rend()) {
            //     rtp_extended_sequence_num_t seq = *it;
            //     spdlog::trace("NACKing seq:{}", seq);

            //     auto offset = seq - nackStartSeq;
            //     if (offset > 15)
            //     {
            //         // Cannot include this seq in our current NACK packet, break to
            //         // send current NACK packet and start a new one
            //         break;
            //     }

            //     bitmask |= 0x1 << offset;

            //     // Premptively mark seq as NACK'd, simpler to mark it here than
            //     // when the packet is actually sent (once this loop breaks)
            //     data.NackQueue.NackSent(seq);

            //     ++it;
            // }

            // // Send NACK packet for bitmask of missing packets
            // sendNack(ssrc, nackStartSeq, bitmask);
        }
    }

    auto packet = RtpPacket(packetBytes, extendedSeq);

    updateMediaPacketStats(packetBytes, data);
    captureVideoKeyframe(packet, data);

    if (onRtpPacketBytes)
    {
        onRtpPacketBytes(packetBytes);
    }
}

void FtlMediaConnection::updateMediaPacketStats(
    const std::vector<std::byte> &packetBytes,
    SsrcData &data)
{
    // Record packet count
    data.PacketsReceived++;

    // Record rolling bytes received
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    if (data.RollingBytesReceivedByTime.count(now) > 0)
    {
        data.RollingBytesReceivedByTime.at(now) += packetBytes.size();
    }
    else
    {
        data.RollingBytesReceivedByTime.emplace(now, packetBytes.size());
    }

    // Trim any packets that are too old from rolling count
    for (auto it = data.RollingBytesReceivedByTime.begin();
         it != data.RollingBytesReceivedByTime.end();)
    {
        uint32_t msDelta =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - it->first).count();
        if (msDelta > rollingSizeAvgMs)
        {
            it = data.RollingBytesReceivedByTime.erase(it);
        }
        else
        {
            // Map is sorted in ascending order - so we can break early
            break;
        }
    }
}

void FtlMediaConnection::captureVideoKeyframe(const RtpPacket &rtpPacket, SsrcData &data)
{
    const RtpHeader *rtpHeader = rtpPacket.Header();
    // Is this a video packet?
    if (ntohl(rtpHeader->Ssrc) == mediaMetadata.VideoSsrc)
    {
        switch (mediaMetadata.VideoCodec)
        {
        case VideoCodecKind::H264:
            captureH264VideoKeyframe(rtpPacket, data);
            break;
        case VideoCodecKind::Unsupported:
        default:
            // We don't know how to process keyframes for this video codec.
            break;
        }
    }
}

void FtlMediaConnection::captureH264VideoKeyframe(const RtpPacket &rtpPacket, SsrcData &data)
{
    // Is this packet part of a keyframe?
    std::span<const std::byte> packetPayload = rtpPacket.Payload();
    if (packetPayload.size() < 2)
    {
        return;
    }
    bool isKeyframePart = false;
    uint8_t nalType = (static_cast<uint8_t>(packetPayload[0]) & 0b00011111);
    if ((nalType == 7) || (nalType == 8)) // Sequence Parameter Set / Picture Parameter Set
    {
        // SPS often precedes an IDR (Instantaneous Decoder Refresh) aka Keyframe
        // and provides information on how to decode it. We should keep this around.
        isKeyframePart = true;
    }
    else if (nalType == 5) // IDR
    {
        // Managed to fit an entire IDR into one packet!
        isKeyframePart = true;
    }
    // See https://tools.ietf.org/html/rfc3984#section-5.8
    else if (nalType == 28 || nalType == 29) // Fragmentation unit (FU-A)
    {
        uint8_t fragmentType = (static_cast<uint8_t>(packetPayload[1]) & 0b00011111);
        if ((fragmentType == 7) || // Fragment of SPS
            (fragmentType == 5))   // Fragment of IDR
        {
            isKeyframePart = true;
        }
    }

    if (!isKeyframePart)
    {
        return;
    }

    if (data.PendingKeyframePackets.size() <= 0)
    {
        insertPacketInSequenceOrder(data.PendingKeyframePackets, rtpPacket);
    }
    else
    {
        const RtpPacket &firstPacket = data.PendingKeyframePackets.front();
        const RtpHeader *firstHeader = firstPacket.Header();
        rtp_timestamp_t lastTimestamp = ntohl(firstHeader->Timestamp);
        rtp_timestamp_t currentTimestamp = ntohl(rtpPacket.Header()->Timestamp);
        if (lastTimestamp == currentTimestamp)
        {
            insertPacketInSequenceOrder(data.PendingKeyframePackets, rtpPacket);
        }
        else
        {
            spdlog::debug("{} keyframe packets recorded @ timestamp {}",
                          data.PendingKeyframePackets.size(), currentTimestamp);
            data.CurrentKeyframePackets.swap(data.PendingKeyframePackets);
            data.PendingKeyframePackets.clear();
            data.PendingKeyframePackets.push_back(rtpPacket);
        }
    }
}

void FtlMediaConnection::sendNack(
    const rtp_ssrc_t ssrc,
    const rtp_extended_sequence_num_t seq,
    const uint16_t followingLostPacketsBitmask)
{
    // See https://tools.ietf.org/html/rfc4585#section-6.2.1
    // for information on how the nack packet is formed
    char nackBuffer[16]{0};
    auto rtcpPacket = reinterpret_cast<RtcpFeedbackPacket *>(nackBuffer);
    rtcpPacket->Header.Version = 2;
    rtcpPacket->Header.Padding = 0;
    rtcpPacket->Header.Rc = RtcpFeedbackMessageType::NACK;
    rtcpPacket->Header.Type = RtcpType::RTPFB;
    rtcpPacket->Header.Length = htons(3);
    rtcpPacket->Ssrc = htonl(ssrc);
    rtcpPacket->Media = htonl(ssrc);
    auto rtcpNack =
        reinterpret_cast<RtcpFeedbackPacketNackControlInfo *>(rtcpPacket->Fci);
    rtcpNack->Pid = htons(seq);
    rtcpNack->Blp = htons(followingLostPacketsBitmask);
    std::vector<std::byte> nackBytes(reinterpret_cast<std::byte *>(nackBuffer),
                                     reinterpret_cast<std::byte *>(nackBuffer) + sizeof(nackBuffer));
    transport->Write(nackBytes);

    spdlog::trace("NACK ssrc:{}, seq:{}, following:{:#016b}", ssrc, seq, followingLostPacketsBitmask);
}

void FtlMediaConnection::handlePing(const std::vector<std::byte> &packetBytes)
{
    // FTL client is trying to measure round trip time (RTT), pong back the same packet
    transport->Write(packetBytes);
}

void FtlMediaConnection::handleSenderReport(const std::vector<std::byte> &packetBytes)
{
    // We expect this packet to be 28 bytes big.
    if (packetBytes.size() != 28)
    {
        spdlog::warn("Invalid sender report packet of length {} (expect 28)", packetBytes.size());
    }
    // char* packet = reinterpret_cast<char*>(rtpHeader);
    // uint32_t ssrc              = ntohl(*reinterpret_cast<uint32_t*>(packet + 4));
    // uint32_t ntpTimestampHigh  = ntohl(*reinterpret_cast<uint32_t*>(packet + 8));
    // uint32_t ntpTimestampLow   = ntohl(*reinterpret_cast<uint32_t*>(packet + 12));
    // uint32_t rtpTimestamp      = ntohl(*reinterpret_cast<uint32_t*>(packet + 16));
    // uint32_t senderPacketCount = ntohl(*reinterpret_cast<uint32_t*>(packet + 20));
    // uint32_t senderOctetCount  = ntohl(*reinterpret_cast<uint32_t*>(packet + 24));

    // uint64_t ntpTimestamp = (static_cast<uint64_t>(ntpTimestampHigh) << 32) |
    //     static_cast<uint64_t>(ntpTimestampLow);

    // TODO: We don't do anything with this information right now, but we ought to log
    // it away somewhere.
}
#pragma endregion