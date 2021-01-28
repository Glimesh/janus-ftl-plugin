/**
 * @file FtlStream.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "FtlStream.h"

#include "ConnectionTransports/ConnectionTransport.h"
#include "FtlControlConnection.h"
#include "JanusSession.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <poll.h>
#include <spdlog/spdlog.h>

#pragma region Constructor/Destructor
FtlStream::FtlStream(
    std::unique_ptr<FtlControlConnection> controlConnection,
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
    onRtpPacket(onRtpPacket),
    nackLostPackets(nackLostPackets)
{
    // Prepare stream data stores to accept packets from SSRCs specified by control handshake
    missedSequenceNumbers.try_emplace(mediaMetadata.AudioSsrc);
    missedSequenceNumbers.try_emplace(mediaMetadata.VideoSsrc);
    packetsSinceLastMissedSequence.try_emplace(mediaMetadata.AudioSsrc, 0);
    packetsSinceLastMissedSequence.try_emplace(mediaMetadata.VideoSsrc, 0);
    circularPacketBuffer.try_emplace(mediaMetadata.AudioSsrc);
    circularPacketBuffer.try_emplace(mediaMetadata.VideoSsrc);

    // Bind to control callbacks
    this->controlConnection->SetOnConnectionClosed(
        std::bind(&FtlStream::controlConnectionClosed, this, std::placeholders::_1));

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
Result<void> FtlStream::StartAsync()
{
    // Fire up our media connection
    Result<void> mediaStartResult = mediaTransport->StartAsync();
    if (mediaStartResult.IsError)
    {
        controlConnection->Stop();
        return mediaStartResult;
    }
    spdlog::info("FTL media stream started for channel {} / stream {}",
        controlConnection->GetChannelId(), streamId);
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

ftl_channel_id_t FtlStream::GetChannelId()
{
    return controlConnection->GetChannelId();
}

ftl_stream_id_t FtlStream::GetStreamId()
{
    return streamId;
}
#pragma endregion

#pragma region Private methods
void FtlStream::controlConnectionClosed(FtlControlConnection& connection)
{
    // Stop the media connection
    mediaTransport->Stop();

    // Indicate that we've been stopped
    onClosed(*this);
}

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
    onClosed(*this);
}

void FtlStream::processRtpPacket(const std::vector<std::byte>& rtpPacket)
{
    std::unique_lock lock(dataMutex);
    const RtpHeader* rtpHeader = reinterpret_cast<const RtpHeader*>(rtpPacket.data());
    rtp_ssrc_t ssrc = ntohl(rtpHeader->ssrc);

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
            ((static_cast<uint8_t>(rtpHeader->markerbit) << 7) | 
            static_cast<uint8_t>(rtpHeader->type));
        
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
            spdlog::warn("FTL: Unknown RTP payload type %d (orig %d)\n", payloadType, 
                rtpHeader->type);
        }
    }
}

void FtlStream::processRtpPacketSequencing(const std::vector<std::byte>& rtpPacket,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    const RtpHeader* rtpHeader = reinterpret_cast<const RtpHeader*>(rtpPacket.data());
    const rtp_ssrc_t ssrc = ntohl(rtpHeader->ssrc);
    const rtp_sequence_num_t seqNum = ntohs(rtpHeader->seq_number);

    if ((circularPacketBuffer.count(ssrc) <= 0) ||
        (missedSequenceNumbers.count(ssrc) <= 0) ||
        (packetsSinceLastMissedSequence.count(ssrc) <= 0))
    {
        const rtp_payload_type_t payloadType = rtpHeader->type;
        spdlog::warn("Received RTP payload type {} with unexpected ssrc {}", payloadType, ssrc);
        return;
    }

    std::set<rtp_sequence_num_t>& ssrcMissedSequenceNumbers = missedSequenceNumbers.at(ssrc);
    size_t& ssrcPacketsSinceLastMissedSequence = packetsSinceLastMissedSequence.at(ssrc);
    std::list<std::vector<std::byte>>& ssrcPacketBuffer = circularPacketBuffer.at(ssrc);
    if (ssrcPacketBuffer.size() != 0)
    {
        // If this sequence is marked as missing, un-mark it.
        ssrcMissedSequenceNumbers.erase(rtpHeader->seq_number);

        // Insert the packet into cache according to sequence number
        for (auto it = ssrcPacketBuffer.rbegin(); it != ssrcPacketBuffer.rend(); it++)
        {
            const std::vector<std::byte>& cachedPacket = *it;
            const RtpHeader* cachedRtpHeader = 
                reinterpret_cast<const RtpHeader*>(cachedPacket.data());
            const rtp_sequence_num_t cachedSeqNum = ntohs(cachedRtpHeader->seq_number);

            if (isSequenceNewer(seqNum, cachedSeqNum, PACKET_BUFFER_SIZE))
            {
                // If there are any gaps between the sequence numbers, mark them as missing.
                // cast to uint16_t to account for overflow (sequence numbers wrap from 65535 -> 0)
                if (cachedSeqNum == static_cast<uint16_t>(seqNum - 1))
                {
                    ssrcPacketsSinceLastMissedSequence++;
                }
                else
                {
                    for (uint16_t missingSeqNum = (cachedSeqNum + 1);
                        (missingSeqNum < seqNum); missingSeqNum++)
                    {
                        ssrcMissedSequenceNumbers.insert(missingSeqNum);
                    }
                    ssrcPacketsSinceLastMissedSequence = 0;
                }
                ssrcPacketBuffer.insert(it.base(), rtpPacket);
                break;
            }
        }
    }
    else
    {
        // We can't check for missing sequences, because this is our first packet!
        ssrcPacketBuffer.push_back(rtpPacket);
    }

    // Trim circular packet buffer to stay within buffer size limit
    if (ssrcPacketBuffer.size() > PACKET_BUFFER_SIZE)
    {
        ssrcPacketBuffer.pop_front();
    }

    if ((ssrcMissedSequenceNumbers.size() >= MAX_PACKETS_BEFORE_NACK) ||
        ((ssrcMissedSequenceNumbers.size() > 0) &&
            (ssrcPacketsSinceLastMissedSequence >= MAX_PACKETS_BEFORE_NACK)))
    {
        sendNacks(ssrc, dataLock);
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

void FtlStream::sendNacks(const rtp_ssrc_t ssrc,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    std::list<std::vector<std::byte>>& packetBuffer = circularPacketBuffer.at(ssrc);
    while (packetBuffer.size() > 0)
    {
        packetBuffer.clear();
        // TODO
        // char nackBuffer[120];
        // RtcpFeedbackPacket* rtcpPacket = reinterpret_cast<RtcpFeedbackPacket*>(nackBuffer);
        // rtcpPacket->header->version = 2;
        // rtcpPacket->header->type = RtcpType::RTPFB;
        // rtcpPacket->header->rc = RtcpFeedbackMessageType::NACK;
    }
}

void FtlStream::processAudioVideoRtpPacket(const std::vector<std::byte>& rtpPacket,
    std::unique_lock<std::shared_mutex>& dataLock)
{
    if (onRtpPacket)
    {
        dataLock.unlock(); // Unlock while we call out
        onRtpPacket(GetChannelId(), streamId, rtpPacket);
        dataLock.lock();
    }
}

void FtlStream::handlePing(const std::vector<std::byte>& rtpPacket)
{
    // These pings are useless - FTL tries to determine 'ping' by having a timestamp
    // sent across and compared against the remote's clock. This assumes that there is
    // no time difference between the client and server, which is practically never true.

    // We'll just ignore these pings, since they wouldn't give us any useful information anyway.
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