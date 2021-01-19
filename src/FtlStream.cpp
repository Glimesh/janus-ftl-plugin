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
    return Result<void>::Success();
}

void FtlStream::Stop()
{
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

    // Parse out RTP packet
    const RtpHeader* rtpHeader = reinterpret_cast<const RtpHeader*>(bytes.data());

    // Process audio/video packets
    if ((rtpHeader->type == mediaMetadata.AudioPayloadType) || 
        (rtpHeader->type == mediaMetadata.VideoPayloadType))
    {
        // Report it out
        if (onRtpPacket)
        {
            onRtpPacket(GetChannelId(), streamId, bytes);
        }
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
            handlePing(bytes);
        }
        else if (payloadType == FTL_PAYLOAD_TYPE_SENDER_REPORT)
        {
            handleSenderReport(bytes);
        }
        else
        {
            spdlog::warn("FTL: Unknown RTP payload type %d (orig %d)\n", payloadType, 
                rtpHeader->type);
        }
    }
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

void FtlStream::handlePing(const std::vector<std::byte>& rtpPacket)
{
    // These pings are useless - FTL tries to determine 'ping' by having a timestamp
    // sent across and compared against the remote's clock. This assumes that there is
    // no time difference between the client and server, which is practically never true.

    // We'll just ignore these pings, since they wouldn't give us any useful information
    // anyway.
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