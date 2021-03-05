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
#include "Utilities/Util.h"

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
    maxMediaPort(maxMediaPort),
    eventQueueThread(std::jthread(&FtlServer::eventQueueThreadBody, this))
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
    // Spin down any active threads
    {
        std::lock_guard stopLock(stoppingMutex);
        isStopping = true;
    }
    stoppingConditionVariable.notify_all();
    // Stop listening for new connections
    ingestControlListener->StopListening();
    // Close any open connections
    std::unique_lock lock(streamDataMutex);
    for (const auto& pendingPair : pendingControlConnections)
    {
        pendingPair.second->Stop();
    }
    pendingControlConnections.clear();
    for (const auto& activePair : activeStreams)
    {
        activePair.second.Stream->Stop();
    }
}

Result<void> FtlServer::StopStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    std::unique_lock lock(streamDataMutex);
    for (const auto& pair : activeStreams)
    {
        const std::unique_ptr<FtlStream>& stream = pair.second.Stream;
        if ((stream->GetChannelId() == channelId) && (stream->GetStreamId() == streamId))
        {
            stream->Stop();
            removeStreamRecord(pair.first, lock);
            return Result<void>::Success();
        }
    }
    return Result<void>::Error("Stream does not exist.");
}

std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
    std::pair<FtlStream::FtlStreamStats, FtlStream::FtlKeyframe>>>
    FtlServer::GetAllStatsAndKeyframes()
{
    std::shared_lock lock(streamDataMutex);
    std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
        std::pair<FtlStream::FtlStreamStats, FtlStream::FtlKeyframe>>>
        returnVal;
    for (const auto& pair : activeStreams)
    {
        const std::unique_ptr<FtlStream>& stream = pair.second.Stream;
        returnVal.emplace_back(std::make_pair(stream->GetChannelId(), stream->GetStreamId()),
            std::make_pair(stream->GetStats(), stream->GetKeyframe()));
    }
    return returnVal;
}

Result<FtlStream::FtlStreamStats> FtlServer::GetStats(ftl_channel_id_t channelId,
    ftl_stream_id_t streamId)
{
    std::shared_lock lock(streamDataMutex);
    for (const auto& pair : activeStreams)
    {
        const std::unique_ptr<FtlStream>& stream = pair.second.Stream;
        if ((stream->GetChannelId() == channelId) && (stream->GetStreamId() == streamId))
        {
            return Result<FtlStream::FtlStreamStats>::Success(stream->GetStats());
        }
    }
    return Result<FtlStream::FtlStreamStats>::Error("Stream does not exist.");
}
#pragma endregion Public functions

#pragma region Private functions
void FtlServer::ingestThreadBody(std::promise<void>&& readyPromise)
{
    ingestControlListener->Listen(std::move(readyPromise));
}

void FtlServer::eventQueueThreadBody()
{
    spdlog::debug("FtlServer::eventQueueThreadBody starting event queue...");
    const std::stop_token stopToken = eventQueueThread.get_stop_token();

    // Bind event listeners
    eventQueue.appendListener(FtlServerEventKind::NewControlConnection,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerNewControlConnectionEvent>)>(
            std::bind(&FtlServer::eventNewControlConnection, this, std::placeholders::_1)));
    eventQueue.appendListener(FtlServerEventKind::ControlStartMediaPort,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerControlStartMediaPortEvent>)>(
            std::bind(&FtlServer::eventControlStartMediaPort, this, std::placeholders::_1)));
    eventQueue.appendListener(FtlServerEventKind::StreamIdAssigned,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerStreamIdAssignedEvent>)>(
            std::bind(&FtlServer::eventStreamIdAssigned, this, std::placeholders::_1)));
    eventQueue.appendListener(FtlServerEventKind::ControlConnectionClosed,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerControlConnectionClosedEvent>)>(
            std::bind(&FtlServer::eventControlConnectionClosed, this, std::placeholders::_1)));
    spdlog::debug("FtlServer::eventQueueThreadBody event listeners bound.");

    // Process event queue until we're asked to stop
    spdlog::debug("FtlServer::eventQueueThreadBody waiting for events...");
    while (true)
    {
        if (stopToken.stop_requested())
        {
            break;
        }
        eventQueue.waitFor(EVENT_QUEUE_WAIT_TIME);
        eventQueue.process();

        // Clean up references to old finished threads
        for (auto i = asyncProcessingThreads.begin(); i != asyncProcessingThreads.end();)
        {
            auto status = i->second.wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready)
            {
                i = asyncProcessingThreads.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }
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

void FtlServer::onNewControlConnection(ConnectionTransport* connection)
{
    spdlog::debug("FtlServer::onNewControlConnection queueing NewControlConnection event");
    eventQueue.enqueue(FtlServerEventKind::NewControlConnection,
        std::shared_ptr<FtlServerNewControlConnectionEvent>(
            new FtlServerNewControlConnectionEvent
            {
                .Connection = connection
            }));
}

std::future<Result<uint16_t>> FtlServer::onControlStartMediaPort(FtlControlConnection* controlConnection,
    ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr)
{
    spdlog::debug("FtlServer::onControlStartMediaPort queueing ControlStartMediaPort event");
    std::promise<Result<uint16_t>> mediaPortPromise;
    auto mediaPortFuture = mediaPortPromise.get_future();
    eventQueue.enqueue(FtlServerEventKind::ControlStartMediaPort,
        std::shared_ptr<FtlServerControlStartMediaPortEvent>(
            new FtlServerControlStartMediaPortEvent
            {
                .MediaPortResultPromise = std::move(mediaPortPromise),
                .Connection = controlConnection,
                .ChannelId = channelId,
                .Metadata = mediaMetadata,
                .TargetAddr = targetAddr,
            }));
    return mediaPortFuture;
}

void FtlServer::onControlConnectionClosed(FtlControlConnection* controlConnection)
{
    spdlog::debug("FtlServer::onControlConnectionClosed queueing ControlConnectionClosed event");
    eventQueue.enqueue(FtlServerEventKind::ControlConnectionClosed,
        std::shared_ptr<FtlServerControlConnectionClosedEvent>(
            new FtlServerControlConnectionClosedEvent
            {
                .Connection = controlConnection
            }));
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

void FtlServer::eventNewControlConnection(std::shared_ptr<FtlServerNewControlConnectionEvent> event)
{
    spdlog::debug("FtlServer::eventNewControlConnection processing NewControlConnection event...");
    auto connection = std::unique_ptr<ConnectionTransport>(event->Connection);
    std::unique_lock lock(streamDataMutex); // TODO: Remove locks when all calls are funneled through message pump
    std::string addrString = connection->GetAddr().has_value() ? 
        Util::AddrToString(connection->GetAddr().value().sin_addr) : "UNKNOWN";
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

    spdlog::info("New FTL control connection is pending from {}", addrString);

    // TODO
    // If this connection doesn't successfully auth in a certain amount of time, close it.
    // auto timeoutThread = std::thread([this, ingestControlConnectionPtr, addrString]() {
    //     std::unique_lock threadLock(stoppingMutex);
    //     stoppingConditionVariable.wait_for(threadLock,
    //         std::chrono::milliseconds(CONNECTION_AUTH_TIMEOUT_MS));
    //     if (isStopping)
    //     {
    //         return;
    //     }

    //     std::unique_lock streamDataLock(streamDataMutex);
    //     if (pendingControlConnections.count(ingestControlConnectionPtr) > 0)
    //     {
    //         spdlog::info("{} didn't authenticate within {}ms, closing",
    //             addrString, CONNECTION_AUTH_TIMEOUT_MS);
    //         pendingControlConnections.at(ingestControlConnectionPtr)->Stop();
    //         pendingControlConnections.erase(ingestControlConnectionPtr);
    //     }
    // });
    // timeoutThread.detach();
}

void FtlServer::eventControlStartMediaPort(std::shared_ptr<FtlServerControlStartMediaPortEvent> event)
{
    spdlog::debug(
        "FtlServer::eventControlStartMediaPort processing ControlStartMediaPort event...");
    
    // Spin up a new processing thread to handle the response from the onStreamStarted callback
    // so we don't hold up our own event queue.
    std::promise<void> threadPromise;
    std::future<void> threadFuture = threadPromise.get_future();
    asyncProcessingThreads.emplace_back(
        std::piecewise_construct,
        std::forward_as_tuple(
            [this, threadPromise = std::move(threadPromise), event] () mutable
            {
                // Attempt to start stream
                Result<ftl_stream_id_t> streamIdResult = 
                    onStreamStarted(event->ChannelId, event->Metadata);
                if (streamIdResult.IsError)
                {
                    event->MediaPortResultPromise.set_value(
                        Result<uint16_t>::Error(streamIdResult.ErrorMessage));
                    return;
                }
                ftl_stream_id_t streamId = streamIdResult.Value;
                
                spdlog::debug("FtlServer::eventControlStartMediaPort processing thread "
                    "successfully received Stream ID - queueing StreamIdAssigned event...");
                eventQueue.enqueue(FtlServerEventKind::StreamIdAssigned,
                    std::shared_ptr<FtlServerStreamIdAssignedEvent>(
                        new FtlServerStreamIdAssignedEvent
                        {
                            .MediaPortResultPromise = std::move(event->MediaPortResultPromise),
                            .Connection = event->Connection,
                            .ChannelId = event->ChannelId,
                            .StreamId = streamId,
                            .Metadata = event->Metadata,
                            .TargetAddr = event->TargetAddr,
                        }));

                threadPromise.set_value();
            }),
        std::forward_as_tuple(std::move(threadFuture)));
}

void FtlServer::eventStreamIdAssigned(std::shared_ptr<FtlServerStreamIdAssignedEvent> event)
{
    FtlControlConnection* controlConnection = event->Connection;
    std::unique_lock lock(streamDataMutex);

    // There's a chance that this control connection was closed before we were able to
    // assign it a media port
    if (pendingControlConnections.count(controlConnection) <= 0)
    {
        spdlog::info("Channel {} / Stream {} control connection was removed before a media port "
            "could be assigned.", event->ChannelId, event->StreamId);
        dispatchOnStreamEnded(event->ChannelId, event->StreamId);
        // If anyone's still listening, tell them something didn't work out
        event->MediaPortResultPromise.set_value(Result<uint16_t>::Error(
            "Control connection was removed before a media port could be assigned"));
        return;
    }

    // Don't erase the connection from the pending store just yet -
    // if there's an error assigning it a port, we need to keep it around so it can handle it!
    std::unique_ptr<FtlControlConnection>& control = 
        pendingControlConnections.at(controlConnection);

    // Attempt to find a free media port to use
    Result<uint16_t> portResult = reserveMediaPort(lock);
    if (portResult.IsError)
    {
        // We were able to start a stream, but we couldn't assign a media port!
        dispatchOnStreamEnded(event->ChannelId, event->StreamId);
        event->MediaPortResultPromise.set_value(portResult);
        return;
    }
    uint16_t mediaPort = portResult.Value;

    // Start a new media connection transport on that port
    std::unique_ptr<ConnectionTransport> mediaTransport = 
        mediaConnectionCreator->CreateConnection(mediaPort, event->TargetAddr);

    // Fire up a new FtlStream and hand over our control connection
    auto stream = std::make_unique<FtlStream>(
        std::move(control), std::move(mediaTransport), event->Metadata, event->StreamId,
        std::bind(&FtlServer::onStreamClosed, this, std::placeholders::_1),
        std::bind(&FtlServer::onStreamRtpPacket, this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3));
    pendingControlConnections.erase(controlConnection);

    Result<void> streamStartResult = stream->StartAsync();
    if (streamStartResult.IsError)
    {
        // Whoops - indicate that the stream we just indicated has started has abruptly ended
        usedMediaPorts.erase(mediaPort);
        dispatchOnStreamEnded(event->ChannelId, event->StreamId);
        event->MediaPortResultPromise.set_value(Result<uint16_t>::Error(
            fmt::format("Couldn't assign media port, FtlStream could not start: {}",
                streamStartResult.ErrorMessage)));
        return;
    }
    activeStreams.try_emplace(stream.get(), std::move(stream), mediaPort);

    // Pass the media port back to the control connection
    spdlog::info("{} FtlStream started streaming Channel {} / Stream {} on port {}", 
        Util::AddrToString(event->TargetAddr), event->ChannelId, event->StreamId, mediaPort);

    event->MediaPortResultPromise.set_value(Result<uint16_t>::Success(mediaPort));
}

void FtlServer::eventControlConnectionClosed(std::shared_ptr<FtlServerControlConnectionClosedEvent> event)
{
    spdlog::debug(
        "FtlServer::eventControlConnectionClosed processing ControlConnectionClosed event...");
    std::unique_lock lock(streamDataMutex);
    if (pendingControlConnections.count(event->Connection) <= 0)
    {
        spdlog::error(
            "Control connection reported closed, but it wasn't found in the pending list.");
        return;
    }
    // Just remove the control connection - the stream hasn't started yet, so we don't
    // need to take care of anything else.
    pendingControlConnections.erase(event->Connection);
    spdlog::info("Pending FTL control connection has closed.");
}

void FtlServer::dispatchOnStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    // Dispatch call to onStreamEnded on a separate thread to avoid blocking our event queue
    std::promise<void> threadPromise;
    std::future<void> threadFuture = threadPromise.get_future();
    asyncProcessingThreads.emplace_back(
        std::piecewise_construct,
        std::forward_as_tuple(
            [this, threadPromise = std::move(threadPromise), channelId, streamId]() mutable
            {
                onStreamEnded(channelId, streamId);
                threadPromise.set_value();
            }),
        std::forward_as_tuple(std::move(threadFuture)));
}
#pragma endregion Private functions