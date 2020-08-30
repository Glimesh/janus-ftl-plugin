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
#include <poll.h>
#include <algorithm>
extern "C"
{
    #include <unistd.h>
    #include <debug.h>
    #include <sys/time.h>
    #include <rtcp.h>
}

#pragma region Constructor/Destructor
FtlStream::FtlStream(
    const std::shared_ptr<IngestConnection> ingestConnection,
    const uint16_t mediaPort,
    const std::shared_ptr<RelayThreadPool> relayThreadPool) : 
    ingestConnection(ingestConnection),
    mediaPort(mediaPort),
    relayThreadPool(relayThreadPool)
{
    // Bind to ingest callbacks
    ingestConnection->SetOnClosed(std::bind(
        &FtlStream::ingestConnectionClosed,
        this,
        std::placeholders::_1));
}
#pragma endregion

#pragma region Public methods
void FtlStream::Start()
{
    // Start listening for incoming packets
    streamThread = std::thread(&FtlStream::startStreamThread, this);
    streamThread.detach();
}

void FtlStream::Stop()
{
    // Stop the ingest connection, which will end up reporting closed to us
    ingestConnection->Stop();
}

void FtlStream::AddViewer(std::shared_ptr<JanusSession> viewerSession)
{
    std::lock_guard<std::mutex> lock(viewerSessionsMutex);
    viewerSessions.push_back(viewerSession);
}

void FtlStream::RemoveViewer(std::shared_ptr<JanusSession> viewerSession)
{
    std::lock_guard<std::mutex> lock(viewerSessionsMutex);
    viewerSessions.erase(
        std::remove(viewerSessions.begin(), viewerSessions.end(), viewerSession),
        viewerSessions.end());
}

void FtlStream::SetOnClosed(std::function<void (FtlStream&)> callback)
{
    onClosed = callback;
}
#pragma endregion

#pragma region Getters/Setters
ftl_channel_id_t FtlStream::GetChannelId()
{
    return ingestConnection->GetChannelId();
}

uint16_t FtlStream::GetMediaPort()
{
    return mediaPort;
}

bool FtlStream::GetHasVideo()
{
    return ingestConnection->GetHasVideo();
}

bool FtlStream::GetHasAudio()
{
    return ingestConnection->GetHasAudio();
}

VideoCodecKind FtlStream::GetVideoCodec()
{
    return ingestConnection->GetVideoCodec();
}

AudioCodecKind FtlStream::GetAudioCodec()
{
    return ingestConnection->GetAudioCodec();
}

uint32_t FtlStream::GetAudioSsrc()
{
    return ingestConnection->GetAudioSsrc();
}

uint32_t FtlStream::GetVideoSsrc()
{
    return ingestConnection->GetVideoSsrc();
}

uint8_t FtlStream::GetAudioPayloadType()
{
    return ingestConnection->GetAudioPayloadType();
}

uint8_t FtlStream::GetVideoPayloadType()
{
    return ingestConnection->GetVideoPayloadType();
}

std::list<std::shared_ptr<JanusSession>> FtlStream::GetViewers()
{
    std::lock_guard<std::mutex> lock(viewerSessionsMutex);
    return viewerSessions;
}
#pragma endregion

#pragma region Private methods
void FtlStream::ingestConnectionClosed(IngestConnection& connection)
{
    // Ingest connection was closed, let's stop this stream.
    stopping = true;
}

void FtlStream::startStreamThread()
{
    sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port = htons(mediaPort);

    mediaSocketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    int bindResult = bind(
        mediaSocketHandle,
        (const sockaddr*)&socketAddress,
        sizeof(socketAddress));

    JANUS_LOG(LOG_INFO, "FTL: Started media connection for %d on port %d\n", GetChannelId(), mediaPort);
    switch (bindResult)
    {
    case 0:
        break;
    case EADDRINUSE:
        throw std::runtime_error("FTL stream could not bind to media socket, "
            "this address is already in use.");
    case EACCES:
        throw std::runtime_error("FTL stream could not bind to media socket, "
            "access was denied.");
    default:
        throw std::runtime_error("FTL stream could not bind to media socket.");
    }

    // Set up some values we'll be using in our read thread
    socklen_t addrlen;
    sockaddr_in remote;
    char buffer[1500] = { 0 };
    int bytesRead = 0;
    // Set up poll
    struct pollfd fds[1];
    fds[0].fd = mediaSocketHandle;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    while (true)
    {
        // Are we stopping?
        if (stopping)
        {
            // Close the socket handle
            close(mediaSocketHandle);
            break;
        }

        int pollResult = poll(fds, 1, 1000);

        if (pollResult < 0)
        {
            // We've lost our socket
            JANUS_LOG(LOG_ERR, "FTL: Unknown media connection read error.\n");
            break;
        }
        else if (pollResult == 0)
        {
            // No new data
            continue;
        }

        // We've got a poll event, let's process it.
        if (fds[0].revents & (POLLERR | POLLHUP))
        {
            JANUS_LOG(LOG_ERR, "FTL: Media connection polling error.\n");
            break;
        }
        else if (fds[0].revents & POLLIN)
        {
            // Ooh, yummy data
            addrlen = sizeof(remote);
            bytesRead = recvfrom(
                mediaSocketHandle,
                buffer,
                sizeof(buffer),
                0,
                (struct sockaddr*)&remote,
                &addrlen);

            if (remote.sin_addr.s_addr != ingestConnection->GetAcceptAddress().sin_addr.s_addr)
            {
                JANUS_LOG(
                    LOG_ERR,
                    "FTL: Channel %u received packet from unexpected address.\n",
                    GetChannelId());
                continue;
            }

            if (bytesRead < 12)
            {
                // This packet is too small to have an RTP header.
                JANUS_LOG(LOG_WARN, "FTL: Channel %u received non-RTP packet.\n", GetChannelId());
                continue;
            }

            // Parse out RTP packet
            janus_rtp_header* rtpHeader = (janus_rtp_header*)buffer;
            rtp_sequence_num_t sequenceNumber = ntohs(rtpHeader->seq_number);

            // Process audio/video packets
            if ((rtpHeader->type == GetAudioPayloadType()) || 
                (rtpHeader->type == GetVideoPayloadType()))
            {
                // Relay the packet
                RtpRelayPacketKind packetKind = rtpHeader->type == GetVideoPayloadType() ? 
                    RtpRelayPacketKind::Video : RtpRelayPacketKind::Audio;
                std::shared_ptr<std::vector<unsigned char>> rtpPacket =
                    std::make_shared<std::vector<unsigned char>>(buffer, buffer + bytesRead);
                relayThreadPool->RelayPacket({
                    .rtpPacketPayload = rtpPacket,
                    .type = packetKind,
                    .channelId = GetChannelId()
                });

                // Check for any lost packets
                markReceivedSequence(rtpHeader->type, sequenceNumber);
                processLostPackets(remote, rtpHeader->type, sequenceNumber, rtpHeader->timestamp);
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
                    handlePing(rtpHeader, bytesRead);
                }
                else if (payloadType == FTL_PAYLOAD_TYPE_SENDER_REPORT)
                {
                    handleSenderReport(rtpHeader, bytesRead);
                }
                else
                {
                    JANUS_LOG(
                        LOG_WARN,
                        "FTL: Unknown RTP payload type %d (orig %d)\n",
                        payloadType,
                        rtpHeader->type);
                }
            }
        }
    }

    // We're no longer listening to incoming packets.
    // TODO: Tell the sessions that we're going away
    if (onClosed != nullptr)
    {
        onClosed(*this);
    }
}

void FtlStream::markReceivedSequence(
    rtp_payload_type_t payloadType,
    rtp_sequence_num_t receivedSequence)
{
    // If this sequence was previously marked as lost, remove it
    if (lostPackets.count(payloadType) > 0)
    {
        auto& lostPayloadPackets = lostPackets[payloadType];
        if (lostPayloadPackets.count(receivedSequence) > 0)
        {
            lostPayloadPackets.erase(receivedSequence);
        }
    }

    // If this is the first sequence, record it
    if (latestSequence.count(payloadType) <= 0)
    {
        latestSequence[payloadType] = receivedSequence;
        JANUS_LOG(
            LOG_INFO,
            "FTL: Channel %u first %u sequence: %u\n",
            GetChannelId(),
            payloadType,
            receivedSequence);
    }
    else
    {
        // Make sure we're actually the latest
        auto lastSequence = latestSequence[payloadType];
        if (lastSequence < receivedSequence)
        {
            // Identify any lost packets between the last sequence
            if (lostPackets.count(payloadType) <= 0)
            {
                lostPackets[payloadType] = std::set<rtp_sequence_num_t>();
            }
            for (auto seq = lastSequence + 1; seq < receivedSequence; ++seq)
            {
                lostPackets[payloadType].insert(seq);
            }
            if ((receivedSequence - lastSequence) > 1)
            {
                JANUS_LOG(
                    LOG_WARN,
                    "FTL: Channel %u PL %u lost %u packets.\n",
                    GetChannelId(),
                    payloadType,
                    (receivedSequence - (lastSequence + 1)));
            }
            latestSequence[payloadType] = receivedSequence;
        }
    }
}

void FtlStream::processLostPackets(
    sockaddr_in remote,
    rtp_payload_type_t payloadType,
    rtp_sequence_num_t currentSequence,
    rtp_timestamp_t currentTimestamp)
{
    if (lostPackets.count(payloadType) > 0)
    {
        auto& lostPayloadPackets = lostPackets[payloadType];
        for (auto it = lostPayloadPackets.cbegin(); it != lostPayloadPackets.cend();)
        {
            const auto& lostPacketSequence = *it;

            // If this 'lost' packet came from the future, get rid of it
            if (lostPacketSequence > currentSequence)
            {
                it = lostPayloadPackets.erase(it);
                continue;
            }

            // Otherwise, ask for re-transmission
            rtp_ssrc_t ssrc;
            if (payloadType == GetVideoPayloadType())
            {
                ssrc = GetVideoSsrc();
            }
            else if (payloadType == GetAudioPayloadType())
            {
                ssrc = GetAudioSsrc();
            }
            else
            {
                JANUS_LOG(
                    LOG_ERR,
                    "FTL: Channel %u cannot NACK unknown payload type %u\n",
                    GetChannelId(),
                    payloadType);
                it = lostPayloadPackets.erase(it);
                continue;
            }

            // See https://tools.ietf.org/html/rfc4585 Section 6.1
            // for information on how the nack packet is formed
            char nackBuf[120];
            janus_rtcp_header *rtcpHeader = reinterpret_cast<janus_rtcp_header*>(nackBuf);
            rtcpHeader->version = 2;
            rtcpHeader->type = RTCP_RTPFB;
            rtcpHeader->rc = 1;
            janus_rtcp_fb* rtcpFeedback = reinterpret_cast<janus_rtcp_fb*>(rtcpHeader);
            rtcpFeedback->media = htonl(ssrc);
            rtcpFeedback->ssrc = htonl(ssrc);
            janus_rtcp_nack* rtcpNack = reinterpret_cast<janus_rtcp_nack*>(rtcpFeedback->fci);
            rtcpNack->pid = htons(lostPacketSequence);
            rtcpNack->blp = 0;
            rtcpHeader->length = htons(3);

            int res = sendto(
                mediaSocketHandle,
                nackBuf,
                16,
                0,
                reinterpret_cast<sockaddr*>(&remote),
                sizeof(remote));
            if (res == -1)
            {
                JANUS_LOG(
                    LOG_ERR,
                    "FTL: Channel %u PL %u NACK failed: %d\n",
                    GetChannelId(),
                    payloadType,
                    errno);
            }

            // Sent! Take it out of the list for now.
            it = lostPayloadPackets.erase(it);

            JANUS_LOG(
                    LOG_INFO,
                    "FTL: Channel %u PL %u sent NACK for seq %u\n",
                    GetChannelId(),
                    payloadType,
                    lostPacketSequence);
        }
    }
}

void FtlStream::handlePing(janus_rtp_header* rtpHeader, uint16_t length)
{
    // These pings are useless - FTL tries to determine 'ping' by having a timestamp
    // sent across and compared against the remote's clock. This assumes that there is
    // no time difference between the client and server, which is practically never true.

    // We'll just ignore these pings, since they wouldn't give us any useful information
    // anyway.
}

void FtlStream::handleSenderReport(janus_rtp_header* rtpHeader, uint16_t length)
{
    // We expect this packet to be 28 bytes big.
    if (length != 28)
    {
        JANUS_LOG(LOG_WARN, "FTL: Invalid sender report packet of length %d (expect 28)\n", length);
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