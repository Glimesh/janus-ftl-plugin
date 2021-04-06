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
#include "Utilities/Rtp.h"

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
    ftl_stream_id_t streamId,
    const ClosedCallback onClosed)
:
    controlConnection(std::move(controlConnection)),
    streamId(streamId),
    onClosed(onClosed)
{
    // Bind to FtlStream
    this->controlConnection->SetFtlStream(this);
}
#pragma endregion

#pragma region Public methods
Result<void> FtlStream::StartMediaConnection(
        std::unique_ptr<ConnectionTransport> mediaTransport,
        const MediaMetadata mediaMetadata,
        const FtlMediaConnection::RtpPacketCallback onRtpPacket)
{
    std::unique_lock lock(mutex);

    // TODO make sure we don't already have a media connection

    mediaConnection = std::make_unique<FtlMediaConnection>(
        std::move(mediaTransport),
        mediaMetadata,
        GetChannelId(),
        GetStreamId(),
        std::bind(&FtlStream::onMediaConnectionClosed, this),
        onRtpPacket
    );

    // TODO Send media port to control connection

    return Result<void>::Success();
}

void FtlStream::Stop()
{
    spdlog::info("Stopping FTL channel {} / stream {}...",
        controlConnection->GetChannelId(),
        streamId);

    // Stop our media connection
    mediaConnection->Stop();

    // Stop the control connection
    controlConnection->Stop();
}

void FtlStream::ControlConnectionStopped(FtlControlConnection* connection)
{
    // Stop the media connection
    mediaConnection->Stop();

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

FtlStreamStats FtlStream::GetStats()
{
    // TODO Don't just assume we have a media connection
    return mediaConnection->GetStats();
}

FtlKeyframe FtlStream::GetKeyframe()
{
    // TODO Don't just assume we have a media connection
    return mediaConnection->GetKeyframe();
}
#pragma endregion

#pragma region Private methods
void FtlStream::onControlConnectionClosed()
{
    mediaConnection->Stop();
    onClosed(this);
}

void FtlStream::onMediaConnectionClosed()
{
    // Somehow our media connection closed - since this transport is usually stateless, we don't
    // expect this to ever happen. Shut everything down nonetheless.
    spdlog::error(
        "Media connection closed unexpectedly for channel {} / stream {}",
        GetChannelId(), streamId);

    controlConnection->Stop();
    onClosed(this);
}

#pragma endregion