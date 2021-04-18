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
        const uint16_t mediaPort,
        const MediaMetadata mediaMetadata,
        const FtlMediaConnection::RtpPacketCallback onRtpPacket)
{
    std::scoped_lock lock(mutex);

    if (mediaConnection)
    {
        return Result<void>::Error("Media connection already started");
    }

    mediaConnection = std::make_unique<FtlMediaConnection>(
        std::move(mediaTransport),
        mediaMetadata,
        GetChannelId(),
        GetStreamId(),
        std::bind(&FtlStream::onMediaConnectionClosed, this),
        onRtpPacket
    );

    // Send media port to control connection
    controlConnection->StartMediaPort(mediaPort);

    return Result<void>::Success();
}

void FtlStream::Stop()
{
    std::scoped_lock lock(mutex);

    spdlog::info("Stopping FTL channel {} / stream {}...",
        controlConnection->GetChannelId(),
        streamId);

    // Stop our media connection if we have one
    if (mediaConnection)
    {
        mediaConnection->Stop();
    }

    // Stop the control connection
    controlConnection->Stop();
}

void FtlStream::ControlConnectionStopped(FtlControlConnection* connection)
{
    std::scoped_lock lock(mutex);
 
    // Stop our media connection if we have one
    if (mediaConnection)
    {
        mediaConnection->Stop();
    }

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

Result<FtlStreamStats> FtlStream::GetStats()
{
    std::scoped_lock lock(mutex);

    if (mediaConnection)
    {
        return Result<FtlStreamStats>::Success(mediaConnection->GetStats());
    }
    else
    {
        return Result<FtlStreamStats>::Error("Stream media connection has not been started");
    }
}

Result<FtlKeyframe> FtlStream::GetKeyframe()
{
    std::scoped_lock lock(mutex);
    
    if (mediaConnection)
    {
        return mediaConnection->GetKeyframe();
    }
    else
    {
        return Result<FtlKeyframe>::Error("Stream media connection has not been started");
    }
}
#pragma endregion

#pragma region Private methods
void FtlStream::onMediaConnectionClosed()
{
    std::scoped_lock lock(mutex);

    // Somehow our media connection closed - since this transport is usually stateless, we don't
    // expect this to ever happen. Shut everything down nonetheless.
    spdlog::error(
        "Media connection closed unexpectedly for channel {} / stream {}",
        GetChannelId(), streamId);
    controlConnection->Stop();
    onClosed(this);
}

#pragma endregion
