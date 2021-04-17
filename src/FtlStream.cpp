/**
 * @file FtlStream.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-08-11
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "FtlStream.h"

#include "ConnectionTransports/ConnectionTransport.h"
#include "FtlControlConnection.h"
#include "JanusSession.h"
#include "Rtp/RtpPacket.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <poll.h>
#include <spdlog/fmt/bin_to_hex.h>

#pragma region Constructor/Destructor
FtlStream::FtlStream(
    std::shared_ptr<FtlControlConnection> controlConnection,
    std::unique_ptr<ConnectionTransport> mediaTransport,
    const MediaMetadata mediaMetadata,
    const ftl_stream_id_t streamId,
    const ClosedCallback onClosed,
    const RtpPacketCallback onRtpPacket,
    const bool nackLostPackets)
:
    controlConnection(std::move(controlConnection)),
    mediaTransport(std::move(mediaTransport)),
    mediaMetadata(mediaMetadata),
    streamId(streamId),
    onClosed(onClosed),
    onRtpPacket(onRtpPacket)
{
    // Prepare stream data stores to accept packets from SSRCs specified by control handshake
    ssrcData.try_emplace(mediaMetadata.AudioSsrc);
    ssrcData.try_emplace(mediaMetadata.VideoSsrc);

    // Bind to FtlStream
    this->controlConnection->SetFtlStream(this);

    // Bind to media transport callbacks
    this->mediaTransport->SetOnBytesReceived(
        std::bind(
            &FtlStream::mediaBytesReceived,
            this,
            std::placeholders::_1));
    this->mediaTransport->SetOnConnectionClosed(std::bind(&FtlStream::mediaConnectionClosed, this));
}
#pragma endregion

#pragma region Public methods
Result<void> FtlStream::StartAsync(uint16_t mediaPort)
{
    std::unique_lock lock(dataMutex);
    // Fire up our media connection
    Result<void> mediaStartResult = mediaTransport->StartAsync();
    if (mediaStartResult.IsError)
    {
        controlConnection->Stop();
        return mediaStartResult;
    }
    // Record start time
    startTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    steadyStartTime = std::chrono::steady_clock::now();
    spdlog::info("Media stream receiving for Channel {} / Stream {}",
        controlConnection->GetChannelId(), streamId);

    // Tell the control connection that we have a media port!
    controlConnection->StartMediaPort(mediaPort);
    return Result<void>::Success();
}

void FtlStream::Stop()
{
    spdlog::info("Stopping FTL channel {} / stream {}...", controlConnection->GetChannelId(),
        streamId);

    // Stop our media connection
    mediaTransport->Stop();

    // Stop the control connection
    controlConnection->Stop();
}

void FtlStream::ControlConnectionStopped(FtlControlConnection* connection)
{
    // Stop the media connection
    mediaTransport->Stop();

    // Indicate that we've been stopped
    if (onClosed)
    {
        onClosed(this);
    }
}

ftl_channel_id_t FtlStream::GetChannelId() const
{
    return controlConnection->GetChannelId();
}

ftl_stream_id_t FtlStream::GetStreamId() const
{
    return streamId;
}

FtlStream::FtlStreamStats FtlStream::GetStats()
{
    std::shared_lock lock(dataMutex);
    FtlStreamStats stats { 0 };
    uint32_t bytesReceived = 0;
    stats.StartTime = startTime;
    stats.DurationSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - steadyStartTime).count();
    for (const auto& dataPair : ssrcData)
    {
        const SsrcData& data = dataPair.second;
        stats.PacketsReceived += data.PacketsReceived;
        stats.PacketsNacked += data.PacketsNacked;
        stats.PacketsLost += data.PacketsLost;
        for (const auto& bytesPair : dataPair.second.RollingBytesReceivedByTime)
        {
            bytesReceived += bytesPair.second;
        }
    }
    stats.RollingAverageBitrateBps = (bytesReceived * 8) / (ROLLING_SIZE_AVERAGE_MS / 1000.0f);

    return stats;
}

FtlStream::FtlKeyframe FtlStream::GetKeyframe()
{
    std::shared_lock lock(dataMutex);
    FtlKeyframe keyframe { mediaMetadata.VideoCodec };
    // Return the last available keyframe for the default video ssrc
    if (ssrcData.count(mediaMetadata.VideoSsrc) <= 0)
    {
        spdlog::error("No ssrc data available for video ssrc {}", mediaMetadata.VideoSsrc);
        return keyframe;
    }
    keyframe.Packets = ssrcData.at(mediaMetadata.VideoSsrc).CurrentKeyframePackets;
    return keyframe;
}
#pragma endregion

#pragma region Private methods
void FtlStream::mediaBytesReceived(const std::vector<std::byte>& bytes)
{
    if (bytes.size() < 12)
    {
        // This packet is too small to have an RTP header.
        spdlog::warn(
            "Channel {} / stream {} received non-RTP packet of size {} (< 12 bytes). Discarding...",
            GetChannelId(), streamId, bytes.size());
        return;
    }

    processRtpPacket(bytes);
}

void FtlStream::mediaConnectionClosed()
{
    // Somehow our media connection closed - since this transport is usually stateless, we don't
    // expect this to ever happen. Shut everything down nonetheless.
    spdlog::error(
        "Media connection closed unexpectedly for channel {} / stream {}",
        GetChannelId(), streamId);
    controlConnection->Stop();
    onClosed(this);
}

std::set<rtp_sequence_num_t> FtlStream::insertPacketInSequenceOrder(
    std::list<std::vector<std::byte>>& packetList, const std::vector<std::byte>& packet)
{
    std::set<rtp_sequence_num_t> missingSequences;
    const RtpHeader* rtpHeader = RtpPacket::GetRtpHeader(packet);
    const rtp_sequence_num_t seqNum = ntohs(rtpHeader->SequenceNumber);
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
        const std::vector<std::byte>& cachedPacket = *it;
        const RtpHeader* cachedRtpHeader = RtpPacket::GetRtpHeader(cachedPacket);
        const rtp_sequence_num_t cachedSeqNum = ntohs(cachedRtpHeader->SequenceNumber);

        if (isSequenceNewer(seqNum, cachedSeqNum))
        {
            // If there are any gaps between the sequence numbers, mark them as missing.
            // cast to uint16_t to account for overflow (sequence numbers wrap from 65535 -> 0)
            if (cachedSeqNum != static_cast<uint16_t>(seqNum - 1))
            {
                for (uint16_t missingSeqNum = (cachedSeqNum + 1);
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
        const std::vector<std::byte>& firstPacket = packetList.front();
        const RtpHeader* firstRtpHeader = RtpPacket::GetRtpHeader(firstPacket);
        const rtp_sequence_num_t firstSeqNum = ntohs(firstRtpHeader->SequenceNumber);

        // Mark in-between packets missing.
        for (uint16_t missingSeqNum = (seqNum + 1);
            (missingSeqNum < firstSeqNum); missingSeqNum++)
        {
            missingSequences.insert(missingSeqNum);
        }
        packetList.push_front(packet);
    }

    return missingSequences;
}

void FtlStream::processRtpPacket(const std::vector<std::byte>& rtpPacket)
{
    std::unique_lock lock(dataMutex);
    const RtpHeader* rtpHeader = RtpPacket::GetRtpHeader(rtpPacket);
    rtp_ssrc_t ssrc = ntohl(rtpHeader->Ssrc);

    // Process audio/video packets
    if ((ssrc == mediaMetadata.AudioSsrc) || 
        (ssrc == mediaMetadata.VideoSsrc))
    {
        processRtpPacketSequencing(rtpPacket, lock);
        processAudioVideoRtpPacket(rtpPacket, lock);
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
            handlePing(rtpPacket);
        }
        else if (payloadType == FTL_PAYLOAD_TYPE_SENDER_REPORT)
        {
            handleSenderReport(rtpPacket);
        }
        else
        {
            spdlog::warn("Unknown RTP payload type {} (orig {})\n", payloadType, 
                rtpHeader->Type);
        }
    }
}

void FtlStream::processRtpPacketSequencing(const std::vector<std::byte>& rtpPacket,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    const RtpHeader* rtpHeader = RtpPacket::GetRtpHeader(rtpPacket);
    const rtp_ssrc_t ssrc = ntohl(rtpHeader->Ssrc);
    const rtp_sequence_num_t seqNum = ntohs(rtpHeader->SequenceNumber);

    if (ssrcData.count(ssrc) <= 0)
    {
        const rtp_payload_type_t payloadType = ntohs(rtpHeader->Type);
        spdlog::warn("Received RTP payload type {} with unexpected ssrc {}", payloadType, ssrc);
        return;
    }

    SsrcData& data = ssrcData.at(ssrc);

    // The FTL client will sometimes send a bunch of audio packets first as a 'speed test'.
    // We should ignore these until we see our first video packet show up.
    if ((ssrc == mediaMetadata.AudioSsrc) &&
        (ssrcData.count(mediaMetadata.VideoSsrc) > 0) &&
        (ssrcData.at(mediaMetadata.VideoSsrc).CircularPacketBuffer.size() <= 0))
    {
        return;
    }

    // If this sequence is marked as missing anywhere, un-mark it.
    data.NackQueue.erase(seqNum);
    data.NackedSequences.erase(seqNum);

    // Tally up the size of the packet
    data.PacketsReceived++;
    std::chrono::time_point<std::chrono::steady_clock> steadyNow = std::chrono::steady_clock::now();
    if (data.RollingBytesReceivedByTime.count(steadyNow) > 0)
    {
        data.RollingBytesReceivedByTime.at(steadyNow) += rtpPacket.size();
    }
    else
    {
        data.RollingBytesReceivedByTime.try_emplace(steadyNow, rtpPacket.size());
    }
    // Trim any packets that are too old
    for (auto it = data.RollingBytesReceivedByTime.begin();
        it != data.RollingBytesReceivedByTime.end();)
    {
        uint32_t msDelta = 
            std::chrono::duration_cast<std::chrono::milliseconds>(steadyNow - it->first).count();
        if (msDelta > ROLLING_SIZE_AVERAGE_MS)
        {
            it = data.RollingBytesReceivedByTime.erase(it);
        }
        else
        {
            // Map is sorted in ascending order - so we can break early
            break;
        }
    }

    // Insert the packet into the buffer in sequence number order
    std::set<rtp_sequence_num_t> missingSequences =
        insertPacketInSequenceOrder(data.CircularPacketBuffer, rtpPacket);

    // Trim circular packet buffer to stay within buffer size limit
    while (data.CircularPacketBuffer.size() > PACKET_BUFFER_SIZE)
    {
        data.CircularPacketBuffer.pop_front();
    }

    // TODO: Disabled NACKs because sometimes on sequence number rollover, we'd start
    // seeing massive amounts of "lost" packets.
    // https://github.com/Glimesh/janus-ftl-plugin/issues/95
    // // Calculate which packets are missing and should be NACK'd
    // rtp_sequence_num_t latestSequence = RtpPacket::GetRtpSequence(data.CircularPacketBuffer.back());
    // if (missingSequences.size() == 0)
    // {
    //     data.PacketsSinceLastMissedSequence++;
    // }
    // else if (missingSequences.size() > (MAX_PACKETS_BEFORE_NACK * 2))
    // {
    //     spdlog::warn("At least {} packets were lost before current sequence {} - ignoring and "
    //         "waiting for stream to stabilize...",
    //         missingSequences.size(), seqNum);
    //     data.PacketsSinceLastMissedSequence = 0;
    //     data.PacketsLost += missingSequences.size();
    // }
    // else
    // {
    //     // Only nack packets if they're reasonably new, and haven't already been nack'd
    //     int missingPacketCount = 0;
    //     for (const auto& missingSeq : missingSequences)
    //     {
    //         if ((data.NackedSequences.count(missingSeq) <= 0) &&
    //             (static_cast<uint16_t>(latestSequence - missingSeq) < NACK_TIMEOUT_SEQUENCE_DELTA))
    //         {
    //             data.NackQueue.insert(missingSeq);
    //             ++missingPacketCount;
    //         }
    //     }
    //     spdlog::debug("Marking {} packets missing since sequence {}", missingPacketCount,
    //         seqNum);
    //     data.PacketsSinceLastMissedSequence = 0;
    // }

    // processNacks(ssrc, dataLock);
    // /TODO
}

void FtlStream::processRtpPacketKeyframe(const std::vector<std::byte>& rtpPacket,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    const RtpHeader* rtpHeader = RtpPacket::GetRtpHeader(rtpPacket);
    // Is this a video packet?
    if (ntohl(rtpHeader->Ssrc) == mediaMetadata.VideoSsrc)
    {
        switch (mediaMetadata.VideoCodec)
        {
        case VideoCodecKind::H264:
            processRtpH264PacketKeyframe(rtpPacket, dataLock);
            break;
        case VideoCodecKind::Unsupported:
        default:
            // We don't know how to process keyframes for this video codec.
            break;
        }
    }
}

void FtlStream::processRtpH264PacketKeyframe(const std::vector<std::byte>& rtpPacket,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    const RtpHeader* rtpHeader = RtpPacket::GetRtpHeader(rtpPacket);
    const rtp_ssrc_t ssrc = ntohl(rtpHeader->Ssrc);
    if (ssrcData.count(ssrc) <= 0)
    {
        spdlog::warn("Couldn't process H264 keyframes for unknown ssrc {}", ssrc);
        return;
    }
    SsrcData& data = ssrcData.at(ssrc);

    // Is this packet part of a keyframe?
    std::span<const std::byte> packetPayload = RtpPacket::GetRtpPayload(rtpPacket);
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
        std::vector<std::byte>& firstPacket = data.PendingKeyframePackets.front();
        const RtpHeader* firstHeader = RtpPacket::GetRtpHeader(firstPacket);
        rtp_timestamp_t lastTimestamp = ntohl(firstHeader->Timestamp);
        rtp_timestamp_t currentTimestamp = ntohl(rtpHeader->Timestamp);
        if (lastTimestamp == currentTimestamp)
        {
            insertPacketInSequenceOrder(data.PendingKeyframePackets, rtpPacket);
        }
        else
        {
            spdlog::debug("{} keyframe packets recorded @ timestamp {}",
                data.PendingKeyframePackets.size(), currentTimestamp);
            data.CurrentKeyframePackets = std::move(data.PendingKeyframePackets); // swap?
            data.PendingKeyframePackets.clear();
            data.PendingKeyframePackets.push_back(rtpPacket);
        }
    }
}

bool FtlStream::isSequenceNewer(rtp_sequence_num_t newSeq, rtp_sequence_num_t oldSeq,
    size_t bufferSize)
{
    // Check for rollover
    uint16_t rolloverDeltaThreshold = (UINT16_MAX - (bufferSize * 2));
    if ((newSeq < oldSeq) && ((oldSeq - newSeq) > rolloverDeltaThreshold))
    {
        return true; 
    }
    else
    {
        return (newSeq > oldSeq);
    }
}

void FtlStream::processNacks(const rtp_ssrc_t ssrc,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    if (ssrcData.count(ssrc) <= 0)
    {
        spdlog::warn("Couldn't process NACKs for unknown ssrc {}", ssrc);
        return;
    }
    SsrcData& data = ssrcData.at(ssrc);
    rtp_sequence_num_t latestSequence = RtpPacket::GetRtpSequence(data.CircularPacketBuffer.back());
    
    // First, toss any old NACK'd packets that we haven't received and mark them lost.
    for (auto it = data.NackedSequences.begin(); it != data.NackedSequences.end();)
    {
        const rtp_sequence_num_t& nackedSeq = *it;
        if (static_cast<uint16_t>(latestSequence - nackedSeq) > NACK_TIMEOUT_SEQUENCE_DELTA)
        {
            data.PacketsLost++;
            it = data.NackedSequences.erase(it);
        }
        else
        {
            break;
        }
    }

    // If enough packets have been marked as missing, or enough time has passed, send NACKs
    if ((data.NackQueue.size() >= MAX_PACKETS_BEFORE_NACK) ||
        ((data.NackQueue.size() > 0) &&
            (data.PacketsSinceLastMissedSequence >= MAX_PACKETS_BEFORE_NACK)))
    {
        spdlog::debug(fmt::format("Sending NACKs for sequences {}",
            fmt::join(data.NackQueue, ", ")));

        // Mark packets as NACK'd
        data.NackedSequences.insert(data.NackQueue.begin(), data.NackQueue.end());
        data.PacketsNacked += data.NackQueue.size();

        // Send the NACK request packets
        while (data.NackQueue.size() > 0)
        {
            auto it = data.NackQueue.begin();
            rtp_sequence_num_t firstSeq = *it;
            uint16_t followingLostPacketsBitmask = 0;
            spdlog::debug("FIRST SEQ {}", firstSeq);
            it++;
            while (true)
            {
                if (it == data.NackQueue.end())
                {
                    break;
                }

                rtp_sequence_num_t nextSeq = *it;
                if ((nextSeq - firstSeq) > 15)
                {
                    break;
                }
                spdlog::debug("SUB SEQ {}", nextSeq);
                followingLostPacketsBitmask |= (0x1 << ((nextSeq - firstSeq) - 1));
                it = data.NackQueue.erase(it);
            }
            data.NackQueue.erase(firstSeq);

            sendNack(ssrc, firstSeq, followingLostPacketsBitmask, dataLock);
        }
    }
}

void FtlStream::sendNack(const rtp_ssrc_t ssrc, const rtp_sequence_num_t packetId,
        const uint16_t followingLostPacketsBitmask,
        const std::unique_lock<std::shared_mutex>& dataLock)
{
    // See https://tools.ietf.org/html/rfc4585#section-6.2.1
    // for information on how the nack packet is formed
    char nackBuffer[16] { 0 };
    auto rtcpPacket = reinterpret_cast<Rtp::RtcpFeedbackPacket*>(nackBuffer);
    rtcpPacket->Header.Version = 2;
    rtcpPacket->Header.Padding = 0;
    rtcpPacket->Header.Rc = Rtp::RtcpFeedbackMessageType::NACK;
    rtcpPacket->Header.Type = Rtp::RtcpType::RTPFB;
    rtcpPacket->Header.Length = htons(3);
    rtcpPacket->Ssrc = htonl(ssrc);
    rtcpPacket->Media = htonl(ssrc);
    auto rtcpNack = 
        reinterpret_cast<Rtp::RtcpFeedbackPacketNackControlInfo*>(rtcpPacket->Fci);
    rtcpNack->Pid = htons(packetId);
    rtcpNack->Blp = htons(followingLostPacketsBitmask);
    std::vector<std::byte> nackBytes(reinterpret_cast<std::byte*>(nackBuffer),
        reinterpret_cast<std::byte*>(nackBuffer) + sizeof(nackBuffer));
    mediaTransport->Write(nackBytes);

    spdlog::debug(
        "SENT {}",
        spdlog::to_hex(nackBytes.begin(), nackBytes.end()));
}

void FtlStream::processAudioVideoRtpPacket(const std::vector<std::byte>& rtpPacket,
    std::unique_lock<std::shared_mutex>& dataLock)
{
    processRtpPacketKeyframe(rtpPacket, dataLock);

    if (onRtpPacket)
    {
        dataLock.unlock(); // Unlock while we call out
        onRtpPacket(rtpPacket);
        dataLock.lock();
    }
}

void FtlStream::handlePing(const std::vector<std::byte>& rtpPacket)
{
    // FTL client is trying to measure round trip time (RTT), pong back the same packet
    mediaTransport->Write(rtpPacket);
}

void FtlStream::handleSenderReport(const std::vector<std::byte>& rtpPacket)
{
    // We expect this packet to be 28 bytes big.
    if (rtpPacket.size() != 28)
    {
        spdlog::warn("Invalid sender report packet of length {} (expect 28)", rtpPacket.size());
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