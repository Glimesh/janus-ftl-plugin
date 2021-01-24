/**
 * @file FtlServer.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-14
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include "FtlServer.h"

#include "ConnectionCreators/ConnectionCreator.h"
#include "ConnectionListeners/ConnectionListener.h"
#include "FtlControlConnection.h"
#include "FtlStream.h"

#include <spdlog/spdlog.h>

#pragma region Constructor/Destructor
FtlServer::FtlServer(
    std::unique_ptr<ConnectionListener> ingestControlListener,
    std::unique_ptr<ConnectionCreator> mediaConnectionCreator,
    RequestKeyCallback onRequestKey,
    StreamStartedCallback onStreamStarted,
    StreamEndedCallback onStreamEnded,
    RtpPacketCallback onRtpPacket,
    uint16_t minMediaPort,
    uint16_t maxMediaPort)
:
    ingestControlListener(std::move(ingestControlListener)),
    mediaConnectionCreator(std::move(mediaConnectionCreator)),
    onRequestKey(onRequestKey),
    onStreamStarted(onStreamStarted),
    onStreamEnded(onStreamEnded),
    onRtpPacket(onRtpPacket),
    minMediaPort(minMediaPort),
    maxMediaPort(maxMediaPort)
{
    this->ingestControlListener->SetOnNewConnection(
        std::bind(&FtlServer::onNewControlConnection, this, std::placeholders::_1));
}
#pragma endregion Constructor/Destructor

#pragma region Public functions
void FtlServer::StartAsync()
{
    // Start listening for new ingest connections
    std::promise<void> listenThreadReadyPromise;
    std::future<void> listenThreadReadyFuture = listenThreadReadyPromise.get_future();
    listenThread = 
        std::thread(&FtlServer::ingestThreadBody, this, std::move(listenThreadReadyPromise));
    listenThread.detach();
    listenThreadReadyFuture.get();
    spdlog::info("FtlServer ready for new connections.");
}

void FtlServer::Stop()
{
    spdlog::info("Stopping FtlServer...");
}

void FtlServer::StopStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    std::unique_lock lock(streamDataMutex);
    for (const auto& pair : activeStreams)
    {
        const std::unique_ptr<FtlStream>& stream = pair.second.Stream;
        if ((stream->GetChannelId() == channelId) && (stream->GetStreamId() == streamId))
        {
            stream->Stop();
            removeStreamRecord(pair.first, lock);
            return;
        }
    }
    throw std::invalid_argument("StopStream called for non-existant channelId and streamId");
}
#pragma endregion Public functions

#pragma region Private functions
void FtlServer::ingestThreadBody(std::promise<void>&& readyPromise)
{
    ingestControlListener->Listen(std::move(readyPromise));
}

Result<uint16_t> FtlServer::reserveMediaPort(const std::unique_lock<std::shared_mutex>& dataLock)
{
    for (uint16_t i = minMediaPort; i <= maxMediaPort; ++i)
    {
        if (usedMediaPorts.count(i) <= 0)
        {
            usedMediaPorts.insert(i);
            return Result<uint16_t>::Success(i);
        }
    }
    return Result<uint16_t>::Error("Could not find an available port.");
}

void FtlServer::removeStreamRecord(FtlStream* stream,
    const std::unique_lock<std::shared_mutex>& dataLock)
{
    // This function removes a stream record, but does not stop the FtlStream.
    if (activeStreams.count(stream) <= 0)
    {
        throw std::invalid_argument("Couldn't remove non-existant stream reference.");
    }
    FtlStreamRecord& record = activeStreams.at(stream);
    // Remove media port reservation
    usedMediaPorts.erase(record.MediaPort);
    // Remove stream record
    activeStreams.erase(stream);
}

void FtlServer::onNewControlConnection(std::unique_ptr<ConnectionTransport> connection)
{
    std::unique_lock lock(streamDataMutex);
    auto ingestControlConnection = std::make_unique<FtlControlConnection>(
        std::move(connection),
        onRequestKey,
        std::bind(&FtlServer::onControlStartMediaPort, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
        std::bind(&FtlServer::onControlConnectionClosed, this, std::placeholders::_1));
    auto ingestControlConnectionPtr = ingestControlConnection.get();
    pendingControlConnections.insert_or_assign(ingestControlConnection.get(),
        std::move(ingestControlConnection));
    ingestControlConnectionPtr->StartAsync();

    spdlog::info("New FTL control connection is pending.");
}

Result<uint16_t> FtlServer::onControlStartMediaPort(FtlControlConnection& controlConnection,
    ftl_channel_id_t channelId, MediaMetadata mediaMetadata,
    sockaddr_in targetAddr)
{
    std::unique_lock lock(streamDataMutex);
    // Locate the control connection in our pending store and pull it out
    if (pendingControlConnections.count(&controlConnection) <= 0)
    {
        throw std::runtime_error("Unknown control connection requested a media port assignment");
    }
    std::unique_ptr<FtlControlConnection> control = 
        std::move(pendingControlConnections[&controlConnection]);
    pendingControlConnections.erase(&controlConnection);

    // Attempt to find a free media port to use
    Result<uint16_t> portResult = reserveMediaPort(lock);
    if (portResult.IsError)
    {
        return portResult;
    }
    uint16_t mediaPort = portResult.Value;

    // Try to start the stream and get a stream ID
    Result<ftl_stream_id_t> streamIdResult = onStreamStarted(channelId, mediaMetadata);
    if (streamIdResult.IsError)
    {
        return Result<uint16_t>::Error(streamIdResult.ErrorMessage);
    }
    ftl_stream_id_t streamId = streamIdResult.Value;

    // Start a new media connection transport on that port
    std::unique_ptr<ConnectionTransport> mediaTransport = 
        mediaConnectionCreator->CreateConnection(mediaPort, targetAddr);

    // Fire up a new FtlStream and hand over our control connection
    auto stream = std::make_unique<FtlStream>(
        std::move(control), std::move(mediaTransport), mediaMetadata, streamId,
        std::bind(&FtlServer::onStreamClosed, this, std::placeholders::_1),
        std::bind(&FtlServer::onStreamRtpPacket, this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3));
    Result<void> streamStartResult = stream->StartAsync();
    if (streamStartResult.IsError)
    {
        return Result<uint16_t>::Error(streamStartResult.ErrorMessage);
    }
    activeStreams.try_emplace(stream.get(), std::move(stream), mediaPort);

    // Pass the media port back to the control connection
    return Result<uint16_t>::Success(mediaPort);
}

void FtlServer::onControlConnectionClosed(FtlControlConnection& controlConnection)
{
    std::unique_lock lock(streamDataMutex);
    spdlog::info("Pending FTL control connection has closed.");
    // We should only receive this event if the stream is still pending.
    if (pendingControlConnections.count(&controlConnection) <= 0)
    {
        throw std::runtime_error("Unknown control connection closed.");
    }
    // Just remove the control connection - the stream hasn't started yet, so we don't
    // need to take care of anything else.
    pendingControlConnections.erase(&controlConnection);
}

void FtlServer::onStreamClosed(FtlStream& stream)
{
    ftl_channel_id_t channelId = 0;
    ftl_stream_id_t streamId = 0;
    {
        std::unique_lock lock(streamDataMutex);
        if (activeStreams.count(&stream) <= 0)
        {
            throw std::runtime_error("Unknown FTL stream closed.");
        }
        channelId = activeStreams.at(&stream).Stream->GetChannelId();
        streamId = activeStreams.at(&stream).Stream->GetStreamId();
        removeStreamRecord(&stream, lock);
    }

    onStreamEnded(channelId, streamId);
}

void FtlServer::onStreamRtpPacket(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
    const std::vector<std::byte>& packet)
{
    onRtpPacket(channelId, streamId, packet);
}
#pragma endregion Private functions