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
    #include <debug.h>
}

#pragma region Constructor/Destructor
FtlStream::FtlStream(std::shared_ptr<IngestConnection> ingestConnection, uint16_t mediaPort) : 
    ingestConnection(ingestConnection),
    mediaPort(mediaPort)
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
uint64_t FtlStream::GetChannelId()
{
    return ingestConnection->GetChannelId();
}

uint16_t FtlStream::GetMediaPort()
{
    return mediaPort;
}

std::list<std::shared_ptr<JanusSession>> FtlStream::GetViewers()
{
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
    struct sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port = htons(mediaPort);

    mediaSocketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    int bindResult = bind(
        mediaSocketHandle,
        (const sockaddr*)&socketAddress,
        sizeof(socketAddress));

    JANUS_LOG(LOG_INFO, "FTL: Started media connection on port %d\n", mediaPort);
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
    struct sockaddr_storage remote;
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

            if (bytesRead < 0 || !janus_is_rtp(buffer, bytesRead))
            {
                // We don't care about non-RTP packets
                continue;
            }

            // Parse out RTP packet
            janus_rtp_header* rtpHeader = (janus_rtp_header*)buffer;

            // FTL designates payload type 97 as audio (Opus)
            if (rtpHeader->type == 97)
            {
                relayRtpPacket({
                    .rtpHeader = rtpHeader,
                    .rtpHeaderLength = static_cast<uint16_t>(bytesRead),
                    .type = RtpRelayPacketKind::Audio,
                });
            }
            // FTL designates payload type 96 as video (H264 or VP8)
            else if (rtpHeader->type == 96)
            {
                
                relayRtpPacket({
                    .rtpHeader = rtpHeader,
                    .rtpHeaderLength = static_cast<uint16_t>(bytesRead),
                    .type = RtpRelayPacketKind::Video,
                });
            }
            else
            {
                JANUS_LOG(LOG_INFO, "FTL: Unknown RTP payload type %d", rtpHeader->type);
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

void FtlStream::relayRtpPacket(RtpRelayPacket rtpPacket)
{
    for (const std::shared_ptr<JanusSession> session : viewerSessions)
    {
        session->RelayRtpPacket(rtpPacket);
    }
}
#pragma endregion