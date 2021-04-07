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
    uint16_t minMediaPort,
    uint16_t maxMediaPort)
:
    ingestControlListener(std::move(ingestControlListener)),
    mediaConnectionCreator(std::move(mediaConnectionCreator)),
    onRequestKey(onRequestKey),
    onStreamStarted(onStreamStarted),
    onStreamEnded(onStreamEnded),
    minMediaPort(minMediaPort),
    maxMediaPort(maxMediaPort),
    eventQueueThread(std::jthread(&FtlServer::eventQueueThreadBody, this))
{
    // Bind event listeners
    eventQueue.appendListener(FtlServerEventKind::StopStream,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerStopStreamEvent>)>(
            std::bind(&FtlServer::eventStopStream, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::NewControlConnection,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerNewControlConnectionEvent>)>(
            std::bind(&FtlServer::eventNewControlConnection, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::ControlConnectionClosed,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerControlConnectionClosedEvent>)>(
            std::bind(&FtlServer::eventControlConnectionClosed, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::ControlRequestHmacKey,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerControlRequestHmacKeyEvent>)>(
            std::bind(&FtlServer::eventControlRequestHmacKey, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::ControlHmacKeyFound,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerControlHmacKeyFoundEvent>)>(
            std::bind(&FtlServer::eventControlHmacKeyFound, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::ControlRequestMediaPort,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerControlRequestMediaPortEvent>)>(
            std::bind(&FtlServer::eventControlRequestMediaPort, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::TerminateControlConnection,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerTerminateControlConnectionEvent>)>(
            std::bind(&FtlServer::eventTerminateControlConnection, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::StreamIdAssigned,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerStreamIdAssignedEvent>)>(
            std::bind(&FtlServer::eventStreamIdAssigned, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::StreamStarted,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerStreamStartedEvent>)>(
            std::bind(&FtlServer::eventStreamStarted, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::StreamStartFailed,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerStreamStartFailedEvent>)>(
            std::bind(&FtlServer::eventStreamStartFailed, this, std::placeholders::_1)));

    eventQueue.appendListener(FtlServerEventKind::StreamClosed,
        eventpp::argumentAdapter<void(std::shared_ptr<FtlServerStreamClosedEvent>)>(
            std::bind(&FtlServer::eventStreamClosed, this, std::placeholders::_1)));

    spdlog::debug("FtlServer::eventQueueThreadBody event listeners bound.");

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
        pendingPair.second.first->Stop();
    }
    pendingControlConnections.clear();
    for (const auto& activePair : activeStreams)
    {
        activePair.second.Stream->Stop();
    }
}

void FtlServer::ControlConnectionStopped(FtlControlConnection* connection)
{
    spdlog::debug("FtlServer::ControlConnectionStopped queueing ControlConnectionClosed event");
    eventQueue.enqueue(FtlServerEventKind::ControlConnectionClosed,
        std::shared_ptr<FtlServerControlConnectionClosedEvent>(
            new FtlServerControlConnectionClosedEvent
            {
                .Connection = connection
            }));
}

void FtlServer::ControlConnectionRequestedHmacKey(FtlControlConnection* connection,
    ftl_channel_id_t channelId)
{
    spdlog::debug("FtlServer::ControlConnectionRequestedHmacKey queueing "
        "ControlRequestHmacKey event");
    eventQueue.enqueue(FtlServerEventKind::ControlRequestHmacKey,
        std::shared_ptr<FtlServerControlRequestHmacKeyEvent>(
            new FtlServerControlRequestHmacKeyEvent
            {
                .Connection = connection,
                .ChannelId = channelId,
            }));
}

void FtlServer::ControlConnectionRequestedMediaPort(FtlControlConnection* connection,
    ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr)
{
    spdlog::debug("FtlServer::ControlConnectionRequestedMediaPort queueing "
        "ControlRequestMediaPort event");
    eventQueue.enqueue(FtlServerEventKind::ControlRequestMediaPort,
        std::shared_ptr<FtlServerControlRequestMediaPortEvent>(
            new FtlServerControlRequestMediaPortEvent
            {
                .Connection = connection,
                .ChannelId = channelId,
                .Metadata = mediaMetadata,
                .TargetAddr = targetAddr,
            }));
}

void FtlServer::StopStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    spdlog::debug("FtlServer::StopStream queueing StopStream event");
    eventQueue.enqueue(FtlServerEventKind::StopStream,
        std::shared_ptr<FtlServerStopStreamEvent>(
            new FtlServerStopStreamEvent
            {
                .ChannelId = channelId,
                .StreamId = streamId,
            }));
}

std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
    std::pair<FtlStreamStats, FtlKeyframe>>>
    FtlServer::GetAllStatsAndKeyframes()
{
    std::shared_lock lock(streamDataMutex);
    std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
        std::pair<FtlStreamStats, FtlKeyframe>>>
        returnVal;
    for (const auto& pair : activeStreams)
    {
        const std::shared_ptr<FtlStream>& stream = pair.second.Stream;
        ftl_channel_id_t channelId = stream->GetChannelId();
        ftl_stream_id_t  streamId = stream->GetStreamId();
        const auto stats = stream->GetStats();
        if (stats.IsError)
        {
            spdlog::debug("No stats available for Channel {} / Stream {}, skipping", channelId, streamId);
            continue;
        }
        const auto keyframe = stream->GetKeyframe();
        if (keyframe.IsError)
        {
            spdlog::debug("No keyframe available for Channel {} / Stream {}, skipping", channelId, streamId);
            continue;
        }
        returnVal.emplace_back(std::make_pair(stream->GetChannelId(), stream->GetStreamId()),
            std::make_pair(stats.Value, keyframe.Value));
    }
    return returnVal;
}

Result<FtlStreamStats> FtlServer::GetStats(ftl_channel_id_t channelId,
    ftl_stream_id_t streamId)
{
    std::shared_lock lock(streamDataMutex);
    for (const auto& pair : activeStreams)
    {
        const std::shared_ptr<FtlStream>& stream = pair.second.Stream;
        if ((stream->GetChannelId() == channelId) && (stream->GetStreamId() == streamId))
        {
            return stream->GetStats();
        }
    }
    return Result<FtlStreamStats>::Error("Stream does not exist.");
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

        // Close any pending control connections that have taken too long to start
        for (auto it = pendingControlConnections.begin(); it != pendingControlConnections.end();)
        {
            if ((std::chrono::steady_clock::now() - it->second.second) > CONNECTION_AUTH_TIMEOUT)
            {
                // Keep a reference alive until we've finished stopping the connection
                std::shared_ptr<FtlControlConnection> expiredControlConnection = 
                    std::move(it->second.first);
                std::string addrString = expiredControlConnection->GetAddr().has_value() ?
                    Util::AddrToString(expiredControlConnection->GetAddr().value().sin_addr) :
                        "UNKNOWN";
                spdlog::info("{} didn't authenticate within {}ms, closing", addrString,
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        CONNECTION_AUTH_TIMEOUT).count());
                it = pendingControlConnections.erase(it);
                dispatchAsyncCall(
                    [connection = std::move(expiredControlConnection)]() mutable
                    {
                        connection->Stop();
                    });
            }
            else
            {
                ++it;
            }
        }

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

Result<uint16_t> FtlServer::reserveMediaPort(
    const std::unique_lock<std::shared_mutex>& dataLock)
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
        throw std::invalid_argument("Couldn't remove non-existent stream reference.");
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

void FtlServer::onStreamClosed(FtlStream* stream)
{
    spdlog::debug("FtlServer::onStreamClosed queueing StreamClosedEvent event");
    eventQueue.enqueue(FtlServerEventKind::StreamClosed,
        std::shared_ptr<FtlServerStreamClosedEvent>(
            new FtlServerStreamClosedEvent
            {
                .Stream = stream
            }));
}

void FtlServer::eventStopStream(std::shared_ptr<FtlServerStopStreamEvent> event)
{
    spdlog::debug("FtlServer::eventStopStream processing StopStream event...");
    std::unique_lock lock(streamDataMutex);
    bool streamFound = false;
    for (auto& pair : activeStreams)
    {
        std::shared_ptr<FtlStream>& stream = pair.second.Stream;
        if ((stream->GetChannelId() == event->ChannelId) &&
            (stream->GetStreamId() == event->StreamId))
        {
            std::shared_ptr<FtlStream> streamRef = std::move(stream);
            removeStreamRecord(pair.first, lock);
            dispatchAsyncCall(
                [streamRef = std::move(streamRef)]() mutable
                {
                    streamRef->Stop();
                });
            streamFound = true;
            break;
        }
    }
    if (!streamFound)
    {
        spdlog::warn("FtlServer::eventStopStream couldn't find stream to remove.");
    }
}

void FtlServer::eventNewControlConnection(std::shared_ptr<FtlServerNewControlConnectionEvent> event)
{
    spdlog::debug("FtlServer::eventNewControlConnection processing NewControlConnection event...");
    auto connection = std::unique_ptr<ConnectionTransport>(event->Connection);
    std::unique_lock lock(streamDataMutex); // TODO: Remove locks when all calls are funneled through message pump
    std::string addrString = connection->GetAddr().has_value() ? 
        Util::AddrToString(connection->GetAddr().value().sin_addr) : "UNKNOWN";
    auto ingestControlConnection = std::make_shared<FtlControlConnection>(this,
        std::move(connection));
    pendingControlConnections.emplace(std::piecewise_construct,
        std::forward_as_tuple(ingestControlConnection.get()),
        std::forward_as_tuple(std::move(ingestControlConnection),
            std::chrono::steady_clock::now()));
    spdlog::info("New FTL control connection is pending from {}", addrString);
}

void FtlServer::eventControlRequestHmacKey(
    std::shared_ptr<FtlServerControlRequestHmacKeyEvent> event)
{
    spdlog::debug(
        "FtlServer::eventControlRequestHmacKey processing ControlRequestHmacKey event...");

    dispatchAsyncCall(
        [this, event]()
        {
            Result<std::vector<std::byte>> keyResult = onRequestKey(event->ChannelId);
            if (keyResult.IsError)
            {
                spdlog::debug("FtlServer::eventControlRequestHmacKey processing thread received "
                    "error fetching HMAC key - queueing TerminateControlConnection event...");
                eventQueue.enqueue(FtlServerEventKind::TerminateControlConnection,
                    std::shared_ptr<FtlServerTerminateControlConnectionEvent>(
                        new FtlServerTerminateControlConnectionEvent
                        {
                            .Connection = event->Connection,
                            .ResponseCode = FtlControlConnection::
                                FtlResponseCode::FTL_INGEST_RESP_INVALID_STREAM_KEY,
                        }));
            }
            else
            {
                spdlog::debug("FtlServer::eventControlRequestHmacKey processing thread "
                    "successfully fetched HMAC key - queueing ControlHmacKeyFound event...");
                eventQueue.enqueue(FtlServerEventKind::ControlHmacKeyFound,
                    std::shared_ptr<FtlServerControlHmacKeyFoundEvent>(
                        new FtlServerControlHmacKeyFoundEvent
                        {
                            .Connection = event->Connection,
                            .HmacKey = keyResult.Value,
                        }));
            }
        });
}

void FtlServer::eventControlHmacKeyFound(std::shared_ptr<FtlServerControlHmacKeyFoundEvent> event)
{
    spdlog::debug(
        "FtlServer::eventControlHmacKeyFound processing ControlHmacKeyFound event...");
    
    if (pendingControlConnections.count(event->Connection) <= 0)
    {
        // If the control connection is already gone, we're done!
        return;
    }

    // Hold a reference to the control connection, just in case it's removed while we're processing
    std::shared_ptr<FtlControlConnection> control = 
        pendingControlConnections.at(event->Connection).first;

    // Send the hmac key to the control connection!
    dispatchAsyncCall(
        [event, control = std::move(control)]() mutable
        {
            control->ProvideHmacKey(event->HmacKey);
        });
}

void FtlServer::eventTerminateControlConnection(
    std::shared_ptr<FtlServerTerminateControlConnectionEvent> event)
{
    spdlog::debug("FtlServer::eventTerminateControlConnection processing "
        "TerminateControlConnection event...");

    if (pendingControlConnections.count(event->Connection) <= 0)
    {
        // If the control connection is already gone, we're done!
        return;
    }

    // Remove the control connection, but hold a reference until we're done disconnecting it
    std::shared_ptr<FtlControlConnection> control = 
        std::move(pendingControlConnections.at(event->Connection).first);
    pendingControlConnections.erase(event->Connection);
    dispatchAsyncCall(
        [event, control = std::move(control)]() mutable
        {
            control->Stop(event->ResponseCode);
        });
}

void FtlServer::eventControlRequestMediaPort(
    std::shared_ptr<FtlServerControlRequestMediaPortEvent> event)
{
    spdlog::debug(
        "FtlServer::eventControlRequestMediaPort processing ControlRequestMediaPort event...");

    // Spin up a new processing thread to handle the response from the onStreamStarted callback
    // so we don't hold up our own event queue.
    dispatchAsyncCall(
        [this, event]()
        {
            // Attempt to start stream
            Result<StartedStreamInfo> streamStartResult = 
                onStreamStarted(event->ChannelId, event->Metadata);
            if (streamStartResult.IsError)
            {
                spdlog::debug("FtlServer::eventControlRequestMediaPort processing thread "
                    "error starting stream - queueing TerminateControlConnection event: {}",
                    streamStartResult.ErrorMessage);
                eventQueue.enqueue(FtlServerEventKind::TerminateControlConnection,
                    std::shared_ptr<FtlServerTerminateControlConnectionEvent>(
                        new FtlServerTerminateControlConnectionEvent
                        {
                            .Connection = event->Connection,
                            .ResponseCode = FtlControlConnection::
                                FtlResponseCode::FTL_INGEST_RESP_SERVER_TERMINATE,
                        }));
            }
            else
            {
                spdlog::debug("FtlServer::eventControlRequestMediaPort processing thread "
                    "successfully received Stream ID - queueing StreamIdAssigned event...");
                ftl_stream_id_t streamId = streamStartResult.Value.StreamId;
                std::shared_ptr<RtpPacketSink> packetSink = streamStartResult.Value.PacketSink;
                eventQueue.enqueue(FtlServerEventKind::StreamIdAssigned,
                    std::shared_ptr<FtlServerStreamIdAssignedEvent>(
                        new FtlServerStreamIdAssignedEvent
                        {
                            .Connection = event->Connection,
                            .ChannelId = event->ChannelId,
                            .StreamId = streamId,
                            .Metadata = event->Metadata,
                            .TargetAddr = event->TargetAddr,
                            .PacketSink = packetSink,
                        }));
            }
        });
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
        return;
    }

    // Don't erase the connection from the pending store just yet -
    // if there's an error assigning it a port, we need to keep it around so it can handle it!
    std::shared_ptr<FtlControlConnection>& control = 
        pendingControlConnections.at(controlConnection).first;

    // Attempt to find a free media port to use
    Result<uint16_t> portResult = reserveMediaPort(lock);
    if (portResult.IsError)
    {
        // We were able to start a stream, but we couldn't assign a media port!
        spdlog::error("FtlServer couldn't assign a media port to Channel {} / Stream {}: {}",
            event->ChannelId, event->StreamId, portResult.ErrorMessage);
        dispatchOnStreamEnded(event->ChannelId, event->StreamId);
        eventQueue.enqueue(FtlServerEventKind::TerminateControlConnection,
            std::shared_ptr<FtlServerTerminateControlConnectionEvent>(
                new FtlServerTerminateControlConnectionEvent
                {
                    .Connection = event->Connection,
                    .ResponseCode = FtlControlConnection::
                        FtlResponseCode::FTL_INGEST_RESP_INTERNAL_SERVER_ERROR,
                }));
        return;
    }
    uint16_t mediaPort = portResult.Value;
    std::shared_ptr<RtpPacketSink> rtpPacketSink = event->PacketSink;

    // Attempt to fire up the new FtlStream. Queue a new event when we're done.
    dispatchAsyncCall(
        [this, event, control = std::move(control), mediaPort, rtpPacketSink]() mutable
        {
            std::unique_ptr<ConnectionTransport> mediaTransport = 
                mediaConnectionCreator->CreateConnection(mediaPort, event->TargetAddr);
            auto stream = std::make_shared<FtlStream>(
                std::move(control),
                event->StreamId,
                std::bind(&FtlServer::onStreamClosed, this, std::placeholders::_1));

            Result<void> streamStartResult = stream->StartMediaConnection(
                std::move(mediaTransport),
                mediaPort,
                event->Metadata,
                [rtpPacketSink](const std::vector<std::byte> packet)
                {
                    rtpPacketSink->SendRtpPacket(packet);
                });
            if (streamStartResult.IsError)
            {
                // Here, we purposefully drop the FtlStream reference since we're done using it.
                spdlog::debug("FtlServer::eventStreamIdAssigned async task is queueing "
                    "StreamStartFailed event due to FtlStream::StartAsync failure: {}",
                    streamStartResult.ErrorMessage);
                eventQueue.enqueue(FtlServerEventKind::StreamStartFailed,
                    std::shared_ptr<FtlServerStreamStartFailedEvent>(
                        new FtlServerStreamStartFailedEvent
                        {
                            .FailureResult = streamStartResult,
                            .ChannelId = event->ChannelId,
                            .StreamId = event->StreamId,
                            .MediaPort = mediaPort,
                            .TargetAddr = event->TargetAddr,
                        }));
                return;
            }

            // Stream was started successfully!
            eventQueue.enqueue(FtlServerEventKind::StreamStarted,
                std::shared_ptr<FtlServerStreamStartedEvent>(
                    new FtlServerStreamStartedEvent
                    {
                        .Stream = stream,
                        .ChannelId = event->ChannelId,
                        .StreamId = event->StreamId,
                        .MediaPort = mediaPort,
                        .TargetAddr = event->TargetAddr,
                    }));
        });

    pendingControlConnections.erase(controlConnection);
}

void FtlServer::eventControlConnectionClosed(
    std::shared_ptr<FtlServerControlConnectionClosedEvent> event)
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

void FtlServer::eventStreamStarted(std::shared_ptr<FtlServerStreamStartedEvent> event)
{
    spdlog::debug("FtlServer::eventStreamStarted processing StreamStarted event...");
    activeStreams.try_emplace(event->Stream.get(), event->Stream, event->MediaPort);
    spdlog::info("{} FtlStream started streaming Channel {} / Stream {} on port {}", 
        Util::AddrToString(event->TargetAddr), event->ChannelId, event->StreamId, event->MediaPort);
}

void FtlServer::eventStreamStartFailed(std::shared_ptr<FtlServerStreamStartFailedEvent> event)
{
    spdlog::debug(
        "FtlServer::eventStreamStartFailed processing StreamStartFailed event...");

    // Free up the media port that was previously occupied by this stream
    usedMediaPorts.erase(event->MediaPort);
    dispatchOnStreamEnded(event->ChannelId, event->StreamId);

    // The stream failed to start, and we never added it to activeStreams, so it will be
    // destructed now.
}

void FtlServer::eventStreamClosed(std::shared_ptr<FtlServerStreamClosedEvent> event)
{
    spdlog::debug("FtlServer::eventStreamClosed processing StreamClosed event...");
    ftl_channel_id_t channelId = 0;
    ftl_stream_id_t streamId = 0;
    {
        std::unique_lock lock(streamDataMutex);
        if (activeStreams.count(event->Stream) <= 0)
        {
            spdlog::error("FtlStream reported closed, but it was not found in the list of "
                "active streams.");
            return;
        }
        channelId = activeStreams.at(event->Stream).Stream->GetChannelId();
        streamId = activeStreams.at(event->Stream).Stream->GetStreamId();
        removeStreamRecord(event->Stream, lock);
    }

    dispatchOnStreamEnded(channelId, streamId);
}

void FtlServer::dispatchOnStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    // Dispatch call to onStreamEnded on a separate thread to avoid blocking our event queue
    dispatchAsyncCall([this, channelId, streamId]()
        {
            onStreamEnded(channelId, streamId);
        });
}
#pragma endregion Private functions