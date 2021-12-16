/**
 * @file JanusFtl.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "Configuration.h"
#include "ConnectionCreators/ConnectionCreator.h"
#include "ConnectionListeners/ConnectionListener.h"
#include "FtlClient.h"
#include "FtlServer.h"
#include "JanusFtl.h"
#include "VideoDecoders/H264VideoDecoder.h"
#include "ServiceConnections/DummyServiceConnection.h"
#include "ServiceConnections/EdgeNodeServiceConnection.h"
#include "ServiceConnections/GlimeshServiceConnection.h"
#include "ServiceConnections/RestServiceConnection.h"
#include "Utilities/JanssonPtr.h"

#include <stdexcept>

extern "C"
{
    #include <apierror.h>
    #include <jansson.h>
    #include <rtcp.h>
}

#pragma region Constructor/Destructor
JanusFtl::JanusFtl(
    janus_plugin* plugin,
    std::unique_ptr<ConnectionListener> ingestControlListener,
    std::unique_ptr<ConnectionCreator> mediaConnectionCreator,
    janus_callbacks* janusCallbacks,
    const char* configPath)
: 
    pluginHandle(plugin),
    janusCore(janusCallbacks)
{
#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::flush_on(spdlog::level::err);

    configuration = std::make_unique<Configuration>();
    configuration->Load();
    maxAllowedBitsPerSecond = configuration->GetMaxAllowedBitsPerSecond();
    rollingSizeAvgMs = configuration->GetRollingSizeAvgMs();
    metadataReportInterval = configuration->GetServiceConnectionMetadataReportInterval();
    watchdog = std::make_unique<Watchdog>(configuration->GetServiceConnectionMetadataReportInterval());
    playoutDelay = configuration->GetPlayoutDelay();

    initVideoDecoders();

    initOrchestratorConnection();

    initServiceConnection();
    
    ftlServer = std::make_unique<FtlServer>(std::move(ingestControlListener),
        std::move(mediaConnectionCreator),
        std::bind(&JanusFtl::ftlServerRequestKey, this, std::placeholders::_1),
        std::bind(&JanusFtl::ftlServerStreamStarted, this, std::placeholders::_1,
            std::placeholders::_2),
        std::bind(&JanusFtl::ftlServerStreamEnded, this, std::placeholders::_1,
            std::placeholders::_2),
        configuration->GetRollingSizeAvgMs(),
        configuration->IsNackLostPacketsEnabled());

    ftlServer->StartAsync();

    initServiceReportThread();

    spdlog::info("FTL plugin initialized!");
    watchdog->Ready();
}

JanusFtl::~JanusFtl()
{
    spdlog::info("Tearing down FTL!");
    {
        std::lock_guard lock(threadShutdownMutex);
        isStopping = true;
    }
    threadShutdownConditionVariable.notify_all();
    serviceReportThreadEndedFuture.wait();
    // TODO: Remove all mountpoints, kill threads, sessions, etc.
    ftlServer->Stop();
}
#pragma endregion

#pragma region Public plugin methods
void JanusFtl::CreateSession(janus_plugin_session* handle, int* error)
{
    std::unique_lock lock(streamDataMutex);
    auto session = std::make_unique<JanusSession>(handle, janusCore, playoutDelay);
    handle->plugin_handle = session.get();
    sessions[handle] = ActiveSession
    {
        .WatchingChannelId = std::nullopt,
        .Session = std::move(session),
    };
}

struct janus_plugin_result* JanusFtl::HandleMessage(janus_plugin_session* handle, char* transaction,
    json_t* message, json_t* jsep)
{
    std::unique_lock lock(streamDataMutex);

    JsonPtr messagePtr(message);
    JsonPtr jsepPtr(jsep);

    // If we're not meant to be streaming to viewers, don't acknowledge any messages.
    if (configuration->GetNodeKind() == NodeKind::Ingest)
    {
        spdlog::warn("Ingest service is ignoring incoming WebRTC message.");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_INVALID_REQUEST,
            "Server is ingest-only.");
    }

    // Look up the session
    if (sessions.count(handle) <= 0)
    {
        spdlog::error("No sessions associated with incoming handle");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_UNKNOWN,
            "No sessions associated with this handle.");
    }
    ActiveSession& session = sessions[handle];

    // Do we have a message?
    if (messagePtr.get() == nullptr)
    {
        spdlog::error("Received empty message!");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_NO_MESSAGE,
            "Received empty message.");
    }
    if (!json_is_object(messagePtr.get()))
    {
        spdlog::error("Error parsing JSON message!");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_INVALID_JSON,
            "Error parsing JSON message.");
    }
    // Should we do JANUS_VALIDATE_JSON_OBJECT here..?

    // Parse out request
    json_t *request = json_object_get(message, "request");
    const char *requestText = json_string_value(request);
    spdlog::info("New request {}", requestText);
    if (std::strcmp(requestText, "watch") == 0)
    {
        return handleWatchMessage(session, std::move(messagePtr), transaction);
    }
    else if (std::strcmp(requestText, "start") == 0)
    {
        return handleStartMessage(session, std::move(messagePtr), transaction);
    }
    else
    {
        spdlog::warn("Unknown request '{}'", requestText);
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_INVALID_REQUEST,
            "Unknown request.");
    }
}

json_t* JanusFtl::HandleAdminMessage(json_t* message)
{
    // TODO: Parse and handle the message.
    return json_object();
}

void JanusFtl::SetupMedia(janus_plugin_session* handle)
{
    std::shared_lock lock(streamDataMutex);
    spdlog::info("SetupMedia");

    if (sessions.count(handle) <= 0)
    {
        spdlog::error("No session associated with this handle");
        return;
    }
    ActiveSession& session = sessions[handle];

    session.Session->SetIsStarted(true);
}

void JanusFtl::IncomingRtp(janus_plugin_session* handle, janus_plugin_rtp* packet)
{
    // We don't care about incoming rtp, we're send-only
}

void JanusFtl::IncomingRtcp(janus_plugin_session* handle, janus_plugin_rtcp* packet)
{
    uint16_t totalLength = packet->length;
    janus_rtcp_header* rtcpHeader = reinterpret_cast<janus_rtcp_header*>(packet->buffer);

    // Turns out these packets often come in big 'ol bundles (compound packets)
    // so let's sort through them.
    while (true)
    {
        switch (rtcpHeader->type)
        {
        case RTCP_RR:
            break;
        case RTCP_PSFB:
            handlePsfbRtcpPacket(handle, rtcpHeader);
            break;
        default:
            spdlog::info("Got unknown RTCP packet! Type: {}", static_cast<uint16_t>(rtcpHeader->type));
            break;
        }

        // Check if we've reached the end of the compound packet, and if not, advance to the next
        uint16_t packetLength = ntohs(rtcpHeader->length);
        if (packetLength == 0)
        {
            break;
        }
        totalLength -= (packetLength * 4) + 4;
        if (totalLength <= 0)
        {
            break;
        }
        rtcpHeader = reinterpret_cast<janus_rtcp_header*>(
            reinterpret_cast<uint32_t*>(rtcpHeader) + packetLength + 1);
    }
}

void JanusFtl::DataReady(janus_plugin_session* handle)
{
    // TODO
    spdlog::info("DataReady");
}

void JanusFtl::HangUpMedia(janus_plugin_session* handle)
{
    // TODO
    spdlog::warn("HangUpMedia called by session, but we're not handling it!");
}

void JanusFtl::DestroySession(janus_plugin_session* handle, int* error)
{
    std::unique_lock lock(streamDataMutex);
    if (sessions.count(handle) == 0)
    {
        spdlog::error("DestroySession: No session associated with this handle");
        *error = -2;
        return;
    }

    ActiveSession& session = sessions[handle];
    if (session.WatchingChannelId.has_value())
    {
        bool orchestratorUnsubscribe = false;
        ftl_channel_id_t channelId = session.WatchingChannelId.value();

        // If session is watching an active stream, remove it
        if (streams.count(channelId) > 0)
        {
            std::shared_ptr<JanusStream>& watchingStream = streams[channelId];
            watchingStream->RemoveViewerSession(session.Session.get());

            // If we're an Edge node and there are no more viewers for this channel, we can
            // unsubscribe.
            if ((configuration->GetNodeKind() == NodeKind::Edge) &&
                (watchingStream->GetViewerCount() == 0))
            {
                orchestratorUnsubscribe = true;
            }
        }

        // If session is pending on an inactive stream, remove it
        if ((pendingViewerSessions.count(channelId) > 0) &&
            (pendingViewerSessions[channelId].count(session.Session.get()) > 0))
        {
            pendingViewerSessions[channelId].erase(session.Session.get());

            // If this was the last pending viewer for this channel, unsubscribe.
            if (pendingViewerSessions[channelId].size() == 0)
            {
                if (configuration->GetNodeKind() == NodeKind::Edge)
                {
                    orchestratorUnsubscribe = true;
                }
                pendingViewerSessions.erase(channelId);
            }
        }

        // Unsubscribe for relays on this channel if this session was the last viewer
        if (orchestratorUnsubscribe)
        {
            // Remove temporary stream key
            const auto& edgeServiceConnection = 
                std::dynamic_pointer_cast<EdgeNodeServiceConnection>(serviceConnection);
            if (edgeServiceConnection == nullptr)
            {
                throw std::runtime_error(
                    "Unexpected service connection type - expected EdgeNodeServiceConnection.");
            }
            edgeServiceConnection->ClearStreamKey(channelId);

            spdlog::info("Last viewer for channel {} has disconnected - unsubscribing...",
                channelId);
            orchestrationClient->SendChannelSubscription(ConnectionSubscriptionPayload
                {
                    .IsSubscribe = false,
                    .ChannelId = channelId,
                });
        }
    }

    // TODO: hang up media
    sessions.erase(handle);
}

json_t* JanusFtl::QuerySession(janus_plugin_session* handle)
{
    // TODO
    return json_object();
}
#pragma endregion

#pragma region Private methods
Result<std::vector<std::byte>> JanusFtl::ftlServerRequestKey(ftl_channel_id_t channelId)
{
    return serviceConnection->GetHmacKey(channelId);
}

Result<FtlServer::StartedStreamInfo> JanusFtl::ftlServerStreamStarted(
    ftl_channel_id_t channelId,
    MediaMetadata mediaMetadata)
{
    std::unique_lock lock(streamDataMutex);

    // Attempt to start the stream on the service connection
    Result<ftl_stream_id_t> startResult = serviceConnection->StartStream(channelId);
    if (startResult.IsError)
    {
        return Result<FtlServer::StartedStreamInfo>::Error(startResult.ErrorMessage);
    }
    ftl_stream_id_t streamId = startResult.Value;

    // Stop any existing streams on this channel
    if (streams.count(channelId) > 0)
    {
        const auto& stream = streams.at(channelId);
        spdlog::info("Existing Stream {} exists for Channel {} - stopping...",
            stream->GetStreamId(), channelId);
        ftlServer->StopStream(stream->GetChannelId(), stream->GetStreamId());
        endStream(stream->GetChannelId(), stream->GetStreamId(), lock);
    }

    // Insert new stream
    auto stream = std::make_shared<JanusStream>(channelId, streamId, mediaMetadata);
    streams[channelId] = stream;

    // Move any pending viewer sessions over
    if (pendingViewerSessions.count(channelId) > 0)
    {
        for (const auto& pendingSession : pendingViewerSessions[channelId])
        {
            stream->AddViewerSession(pendingSession);
            sendJsep(sessions[pendingSession->GetJanusPluginSessionHandle()], *stream, nullptr);
        }
        pendingViewerSessions.erase(channelId);
    }
    // TODO: Notify viewer sessions

    // If we are configured as an Ingest node, notify the Orchestrator that a stream has started.
    if ((configuration->GetNodeKind() == NodeKind::Ingest) && (orchestrationClient != nullptr))
    {
        spdlog::info("Publishing channel {} / stream {} to Orchestrator...", channelId,
            streamId);
        orchestrationClient->SendStreamPublish(ConnectionPublishPayload
            {
                .IsPublish = true,
                .ChannelId = channelId,
                .StreamId = streamId,
            });
    }

    spdlog::info("Registered new stream: Channel {} / Stream {}.", channelId, streamId);

    return Result<FtlServer::StartedStreamInfo>::Success(FtlServer::StartedStreamInfo {
        .StreamId = streamId,
        .PacketSink = stream,
    });
}

void JanusFtl::ftlServerStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    std::unique_lock lock(streamDataMutex);
    endStream(channelId, streamId, lock);
}

void JanusFtl::initVideoDecoders()
{
    // H264
    videoDecoders.try_emplace(VideoCodecKind::H264, std::make_unique<H264VideoDecoder>());
}

void JanusFtl::initOrchestratorConnection()
{
    if (configuration->GetNodeKind() != NodeKind::Standalone)
    {
        spdlog::info(
            "Connecting to Orchestration service @ {}:{}...",
            configuration->GetOrchestratorHostname().c_str(),
            configuration->GetOrchestratorPort());
        
        // Open Orchestrator connection
        orchestrationClient = FtlOrchestrationClient::Connect(
            configuration->GetOrchestratorHostname(),
            configuration->GetOrchestratorPsk(),
            configuration->GetMyHostname(),
            configuration->GetOrchestratorPort());
        
        // Bind to events from Orchestrator connection
        orchestrationClient->SetOnConnectionClosed(
            std::bind(&JanusFtl::onOrchestratorConnectionClosed, this));
        orchestrationClient->SetOnIntro(
            std::bind(&JanusFtl::onOrchestratorIntro, this, std::placeholders::_1));
        orchestrationClient->SetOnOutro(
            std::bind(&JanusFtl::onOrchestratorOutro, this, std::placeholders::_1));
        orchestrationClient->SetOnStreamRelay(
            std::bind(&JanusFtl::onOrchestratorStreamRelay, this, std::placeholders::_1));

        // Start the connection and send an Intro
        orchestrationClient->Start();
        orchestrationClient->SendIntro(ConnectionIntroPayload
            {
                .VersionMajor = 0,
                .VersionMinor = 0,
                .VersionRevision = 0,
                .RelayLayer = 0,
                .RegionCode = configuration->GetOrchestratorRegionCode(),
                .Hostname = configuration->GetMyHostname(),
            });
    }
}

void JanusFtl::initServiceConnection()
{
    // If we are configured to be an edge node, we *must* use the EdgeNodeServiceConnection
    if (configuration->GetNodeKind() == NodeKind::Edge)
    {
        serviceConnection = std::make_shared<EdgeNodeServiceConnection>();
    }
    else
    {
        switch (configuration->GetServiceConnectionKind())
        {
        case ServiceConnectionKind::GlimeshServiceConnection:
            serviceConnection = std::make_shared<GlimeshServiceConnection>(
                configuration->GetGlimeshServiceHostname(),
                configuration->GetGlimeshServicePort(),
                configuration->GetGlimeshServiceUseHttps(),
                configuration->GetGlimeshServiceClientId(),
                configuration->GetGlimeshServiceClientSecret());
            break;
        case ServiceConnectionKind::RestServiceConnection:
            serviceConnection = std::make_shared<RestServiceConnection>(
                configuration->GetRestServiceHostname(),
                configuration->GetRestServicePort(),
                configuration->GetRestServiceUseHttps(),
                configuration->GetRestServicePathBase(),
                configuration->GetRestServiceAuthToken());
            break;
        case ServiceConnectionKind::DummyServiceConnection:
        default:
            serviceConnection = std::make_shared<DummyServiceConnection>(
                configuration->GetDummyHmacKey(),
                configuration->GetDummyPreviewImagePath());
            break;
        }
    }

    serviceConnection->Init();
}

void JanusFtl::initServiceReportThread()
{
    std::promise<void> serviceReportThreadEndedPromise;
    serviceReportThreadEndedFuture = serviceReportThreadEndedPromise.get_future();
    serviceReportThread = std::thread(
        &JanusFtl::serviceReportThreadBody,
        this,
        std::move(serviceReportThreadEndedPromise));
    serviceReportThread.detach();
}

void JanusFtl::serviceReportThreadBody(std::promise<void>&& threadEndedPromise)
{
    threadEndedPromise.set_value_at_thread_exit();
    std::unique_lock lock(threadShutdownMutex);

    while (true)
    {
        watchdog->IAmAlive();

        threadShutdownConditionVariable.wait_for(lock, metadataReportInterval);

        if (isStopping)
        {
            break;
        }

        // Quickly gather data from active streams while under lock (defer reporting to avoid
        // holding up other threads)
        std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
            std::pair<FtlStreamStats, FtlKeyframe>>> statsAndKeyframes = 
                ftlServer->GetAllStatsAndKeyframes();
        std::unordered_map<ftl_channel_id_t, MediaMetadata> metadataByChannel;
        std::unordered_map<ftl_channel_id_t, uint32_t> viewersByChannel;
        std::unique_lock lock(streamDataMutex);
        for (const auto& streamInfo : statsAndKeyframes)
        {
            const ftl_channel_id_t& channelId = streamInfo.first.first;
            if (streams.count(channelId) <= 0)
            {
                continue;
            }
            metadataByChannel.try_emplace(channelId, streams.at(channelId)->GetMetadata());
            viewersByChannel.try_emplace(channelId, streams.at(channelId)->GetViewerCount());
        }
        lock.unlock();

        // Now coalesce all of the stream data and report it to the ServiceConnection
        std::list<std::pair<ftl_channel_id_t, ftl_stream_id_t>> streamsStopped;
        for (const auto& streamInfo : statsAndKeyframes)
        {
            const ftl_channel_id_t& channelId = streamInfo.first.first;
            const ftl_stream_id_t& streamId = streamInfo.first.second;
            const FtlStreamStats& stats = streamInfo.second.first;
            const FtlKeyframe& keyframe = streamInfo.second.second;

            // Has this stream exceeded the maximum allowed bandwidth?
            if ((maxAllowedBitsPerSecond > 0) && 
                (stats.RollingAverageBitrateBps > maxAllowedBitsPerSecond))
            {
                spdlog::info("Channel {} / Stream {} is averaging {}bps, exceeding the limit of "
                    "{}bps. Stopping the stream...", channelId, streamId,
                    stats.RollingAverageBitrateBps, maxAllowedBitsPerSecond);
                ftlServer->StopStream(channelId, streamId);
                streamsStopped.emplace_back(channelId, streamId);
                continue;
            }

            if ((viewersByChannel.count(channelId) <= 0) ||
                (metadataByChannel.count(channelId) <= 0))
            {
                continue;
            }
            const MediaMetadata& mediaMetadata = metadataByChannel.at(channelId);
            const uint32_t& numActiveViewers = viewersByChannel.at(channelId);

            // Fallback width/height values, usually wrong
            uint16_t videoWidth = mediaMetadata.VideoWidth;
            uint16_t videoHeight = mediaMetadata.VideoHeight;

            // Do we have a videodecoder available for this stream's codec?
            if ((keyframe.Packets.size() > 0) && (videoDecoders.count(keyframe.Codec) > 0))
            {
                try
                {
                    // Read correct video dimensions
                    std::pair<uint16_t, uint16_t> widthHeight =
                        videoDecoders.at(keyframe.Codec)->ReadVideoDimensions(keyframe.Packets);

                    videoWidth = widthHeight.first;
                    videoHeight = widthHeight.second;
                }
                catch (const PreviewGenerationFailedException& e)
                {
                    spdlog::warn("Couldn't read stream video size for channel {} / stream {}: {}",
                        channelId, streamId, e.what());
                }
            }

            StreamMetadata metadata
            {
                .ingestServerHostname = configuration->GetMyHostname(),
                .streamTimeSeconds = stats.DurationSeconds,
                .numActiveViewers = numActiveViewers,
                .currentSourceBitrateBps = stats.RollingAverageBitrateBps,
                .numPacketsReceived = stats.PacketsReceived,
                .numPacketsNacked = stats.PacketsNacked,
                .numPacketsLost = stats.PacketsLost,
                .streamerToIngestPingMs = 0, // TODO
                .streamerClientVendorName = mediaMetadata.VendorName,
                .streamerClientVendorVersion = mediaMetadata.VendorVersion,
                .videoCodec = SupportedVideoCodecs::VideoCodecString(mediaMetadata.VideoCodec),
                .audioCodec = SupportedAudioCodecs::AudioCodecString(mediaMetadata.AudioCodec),
                .videoWidth = videoWidth,
                .videoHeight = videoHeight,
            };
            Result<ServiceConnection::ServiceResponse> updateResult =
                serviceConnection->UpdateStreamMetadata(streamId, metadata);
            // Check if the request failed, or the service wants to end this stream
            if (updateResult.IsError || 
                (updateResult.Value == ServiceConnection::ServiceResponse::EndStream))
            {
                if (updateResult.IsError)
                {
                    spdlog::info("Service metadata update for Channel {} / Stream {} failed, "
                        "ending stream: {}", channelId, streamId, updateResult.ErrorMessage);
                }
                else
                {
                    spdlog::info("Service requested to end Channel {} / Stream {}. "
                        "Stopping the stream...", channelId, streamId);
                }

                ftlServer->StopStream(channelId, streamId);
                streamsStopped.emplace_back(channelId, streamId);
                continue;
            }

            // Do we have a videodecoder available for this stream's codec?
            if ((keyframe.Packets.size() > 0) && (videoDecoders.count(keyframe.Codec) > 0))
            {
                try
                {
                    std::vector<uint8_t> jpegBytes =
                        videoDecoders.at(keyframe.Codec)->GenerateJpegImage(keyframe.Packets);
                    serviceConnection->SendJpegPreviewImage(streamId, jpegBytes);
                }
                catch (const PreviewGenerationFailedException& e)
                {
                    spdlog::warn("Couldn't generate preview for channel {} / stream {}: {}",
                        channelId, streamId, e.what());
                }
            }
        }

        // Acquire lock and clean up any streams that were stopped
        // We do this last to avoid locking while calling FtlStream::Stop(), since this call could
        // wind up waiting forever on the connection thread due to it taking a lock in the
        // JanusFtl::ftlServerRtpPacket callback
        lock.lock();
        for (const auto& channelStreamPair : streamsStopped)
        {
            endStream(channelStreamPair.first, channelStreamPair.second, lock);
        }
    }
}

void JanusFtl::endStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
    const std::unique_lock<std::shared_mutex>& streamDataLock)
{
    if (streams.count(channelId) <= 0)
    {
        spdlog::error("Received stream ended from unknown channel {} / stream {}", channelId,
            streamId);
        return;
    }
    const std::shared_ptr<JanusStream>& stream = streams.at(channelId);
    if (stream->GetStreamId() != streamId)
    {
        spdlog::error("Stream ended from channel {} had unexpected stream id {}, expected {}",
            channelId, streamId, stream->GetStreamId());
        return;
    }

    // Reset any existing viewers to a pending state
    auto viewerSessions = stream->RemoveAllViewerSessions();
    pendingViewerSessions[channelId].insert(viewerSessions.begin(), viewerSessions.end());
    // TODO: Tell viewers stream is offline.

    // If we are configured as an Ingest node, notify the Orchestrator that a stream has ended.
    if ((configuration->GetNodeKind() == NodeKind::Ingest) && (orchestrationClient != nullptr))
    {
        spdlog::info("Unpublishing channel {} / stream {} from Orchestrator",
            stream->GetChannelId(), stream->GetStreamId());
        orchestrationClient->SendStreamPublish(ConnectionPublishPayload
            {
                .IsPublish = false,
                .ChannelId = stream->GetChannelId(),
                .StreamId = stream->GetStreamId(),
            });
    }

    stream->StopRelays();

    spdlog::info("Stream ended. Channel {} / stream {}",
        stream->GetChannelId(), stream->GetStreamId());

    serviceConnection->EndStream(streamId);
    streams.erase(channelId);
}

void JanusFtl::handlePsfbRtcpPacket(janus_plugin_session* handle, janus_rtcp_header* packet)
{
    switch (packet->rc)
    {
    case 1:
    {
        // PLI - send keyframe?
        break;
    }
    default:
        break;
    }
}

janus_plugin_result* JanusFtl::generateMessageErrorResponse(int errorCode, std::string errorMessage)
{
    json_t *event = json_object();
    json_object_set_new(event, "streaming", json_string("event"));
    json_object_set_new(event, "error_code", json_integer(errorCode));
    json_object_set_new(event, "error", json_string(errorMessage.c_str()));
    return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, event);
}

janus_plugin_result* JanusFtl::handleWatchMessage(ActiveSession& session, JsonPtr message,
    char* transaction)
{
    // We are already holding a unique lock on streamDataMutex
    
    json_t* channelIdJs = json_object_get(message.get(), "channelId");
    if ((channelIdJs == nullptr) || !json_is_integer(channelIdJs))
    {
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_MISSING_ELEMENT,
            "Expected 'channelId' property with integer value.");
    }
    uint32_t channelId = json_integer_value(channelIdJs);

    // Look up the stream associated with given channel ID
    spdlog::info("Request to watch channel {}", channelId);
    session.WatchingChannelId = channelId;
    if (streams.count(channelId) <= 0)
    {
        // This channel doesn't have a stream running!
        int pendingViewers = 0;
        if (pendingViewerSessions.count(channelId) > 0)
        {
            pendingViewers = pendingViewerSessions[channelId].size();
        }

        // If we're an Edge node and this is a first viewer for a given channel,
        // request that this channel be relayed to us.
        if ((configuration->GetNodeKind() == NodeKind::Edge) && (pendingViewers == 0))
        {
            // Generate a new stream key for incoming relay of this channel
            const auto& edgeServiceConnection = 
                std::dynamic_pointer_cast<EdgeNodeServiceConnection>(serviceConnection);
            if (edgeServiceConnection == nullptr)
            {
                throw std::runtime_error(
                    "Unexpected service connection type - expected EdgeNodeServiceConnection.");
            }
            std::vector<std::byte> streamKey = edgeServiceConnection->ProvisionStreamKey(channelId);

            // Subscribe for relay of this stream
            spdlog::info("First viewer for channel {} - subscribing...",
               channelId);
            orchestrationClient->SendChannelSubscription(ConnectionSubscriptionPayload
                {
                    .IsSubscribe = true,
                    .ChannelId = channelId,
                    .StreamKey = streamKey,
                });
        }

        // Add this session to a pending viewership list.
        spdlog::info("No current stream for channel {} - viewer session is pending.", channelId);
        if (pendingViewerSessions.count(channelId) <= 0)
        {
            pendingViewerSessions.insert_or_assign(channelId, std::unordered_set<JanusSession*>());
        }
        pendingViewerSessions[channelId].insert(session.Session.get());

        // Tell the client that we're pending an active stream
        JsonPtr eventPtr(json_object());
        json_object_set_new(eventPtr.get(), "streaming", json_string("event"));
        json_t* result = json_object();
        json_object_set_new(result, "status", json_string("pending"));
        json_object_set_new(eventPtr.get(), "result", result);

        janusCore->push_event(
            session.Session->GetJanusPluginSessionHandle(),
            pluginHandle,
            transaction,
            eventPtr.get(),
            nullptr);

        return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
    }

    // Otherwise, we've got a live stream!
    auto& stream = streams.at(channelId);

    // TODO allow user to request ICE restart (new offer)

    // TODO if they're already watching, handle it

    // Set this session as a viewer
    stream->AddViewerSession(session.Session.get());

    // Send the JSEP to initiate the media connection
    sendJsep(session, *stream, transaction);

    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

janus_plugin_result* JanusFtl::handleStartMessage(ActiveSession& session, JsonPtr message,
    char* transaction)
{
    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

int JanusFtl::sendJsep(const ActiveSession& session, const JanusStream& stream, char* transaction)
{
    // Prepare JSEP payload
    std::string sdpOffer = generateSdpOffer(session, stream);
    JsonPtr jsepPtr(json_pack("{ssss}", "type", "offer", "sdp", sdpOffer.c_str()));

    // Prepare message response
    JsonPtr eventPtr(json_object());
    json_object_set_new(eventPtr.get(), "streaming", json_string("event"));
    json_t* result = json_object();
    json_object_set_new(result, "status", json_string("preparing"));
    json_object_set_new(eventPtr.get(), "result", result);

    // Push response
    return janusCore->push_event(
        session.Session->GetJanusPluginSessionHandle(),
        pluginHandle,
        transaction,
        eventPtr.get(),
        jsepPtr.get());
}

std::string JanusFtl::generateSdpOffer(const ActiveSession& session, const JanusStream& stream)
{
    // https://tools.ietf.org/html/rfc4566

    std::stringstream offerStream;

    // Session description
    offerStream <<  
        "v=0\r\n" <<
        "o=- " << session.Session->GetSdpSessionId() << " " << session.Session->GetSdpVersion() << " IN IP4 127.0.0.1\r\n" <<
        "s=Channel " << stream.GetChannelId() << "\r\n";

    // Audio media description
    if (stream.GetMetadata().HasAudio)
    {
        std::string audioPayloadType = std::to_string(stream.GetMetadata().AudioPayloadType);
        std::string audioCodec = 
            SupportedAudioCodecs::AudioCodecString(stream.GetMetadata().AudioCodec);
        offerStream <<  
            "m=audio 1 RTP/SAVPF " << audioPayloadType << "\r\n" <<
            "c=IN IP4 1.1.1.1\r\n" <<
            "a=rtpmap:" << audioPayloadType << " " << audioCodec << "/48000/2\r\n" <<
            "a=fmtp:" << audioPayloadType << " sprop-stereo=1;\r\n" <<
            "a=sendonly\r\n" <<
            "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";
    }

    // Video media description
    if (stream.GetMetadata().HasVideo)
    {
        std::string videoPayloadType = std::to_string(stream.GetMetadata().VideoPayloadType);
        std::string videoCodec = 
            SupportedVideoCodecs::VideoCodecString(stream.GetMetadata().VideoCodec);
        offerStream <<  
            "m=video 1 RTP/SAVPF " << videoPayloadType << "\r\n" <<
            "c=IN IP4 1.1.1.1\r\n" <<
            "a=rtpmap:" << videoPayloadType << " " << videoCodec << "/90000\r\n" <<
            "a=fmtp:" << videoPayloadType << " profile-level-id=42e01f;packetization-mode=1;\r\n"
            "a=rtcp-fb:" << videoPayloadType << " nack\r\n" <<            // Send us NACK's
            "a=rtcp-fb:" << videoPayloadType << " nack pli\r\n" <<        // Send us picture-loss-indicators
            // "a=rtcp-fb:96 nack goog-remb\r\n" <<  // Send some congestion indicator thing
            "a=sendonly\r\n" <<
            "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n" <<
            "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n";
    }
    return offerStream.str();
}

void JanusFtl::onOrchestratorConnectionClosed()
{
    // TODO: We should reconnect.
    throw std::runtime_error("Connection to Orchestrator was closed unexpectedly.");
}

ConnectionResult JanusFtl::onOrchestratorIntro(ConnectionIntroPayload payload)
{
    spdlog::info("Received Intro from Orchestrator.\n");
    return ConnectionResult
    {
        .IsSuccess = true,
    };
}

ConnectionResult JanusFtl::onOrchestratorOutro(ConnectionOutroPayload payload)
{
    spdlog::info("Received Outro from Orchestrator: {}", payload.DisconnectReason);

    return ConnectionResult
    {
        .IsSuccess = true,
    };
}

ConnectionResult JanusFtl::onOrchestratorStreamRelay(ConnectionRelayPayload payload)
{
    std::unique_lock lock(streamDataMutex);
    if (payload.IsStartRelay)
    {
        spdlog::info("Start Stream Relay request from Orchestrator: "
            "Channel {}, Stream {}, Target {}", payload.ChannelId, payload.StreamId,
            payload.TargetHostname);

        // Do we have an active stream?
        if (!streams.contains(payload.ChannelId))
        {
            spdlog::error("Orchestrator requested a relay for channel that is not streaming."
                "Target hostname: {}, Channel ID: {}", payload.TargetHostname, payload.ChannelId);
            return ConnectionResult
                {
                    .IsSuccess = false,
                };
        }
        auto& stream = streams.at(payload.ChannelId);

        // Start the relay now!
        auto relayClient = std::make_unique<FtlClient>(payload.TargetHostname, payload.ChannelId,
            payload.StreamKey);
        Result<void> connectResult = relayClient->ConnectAsync(FtlClient::ConnectMetadata
            {
                .VendorName = "janus-ftl-plugin",
                .VendorVersion = "0.0.0", // TODO: Versioning
                .HasVideo = stream->GetMetadata().HasVideo,
                .VideoCodec = SupportedVideoCodecs::VideoCodecString(
                    stream->GetMetadata().VideoCodec),
                .VideoHeight = stream->GetMetadata().VideoHeight,
                .VideoWidth = stream->GetMetadata().VideoWidth,
                .VideoPayloadType = stream->GetMetadata().VideoPayloadType,
                .VideoIngestSsrc = stream->GetMetadata().VideoSsrc,
                .HasAudio = stream->GetMetadata().HasAudio,
                .AudioCodec = SupportedAudioCodecs::AudioCodecString(
                    stream->GetMetadata().AudioCodec),
                .AudioPayloadType = stream->GetMetadata().AudioPayloadType,
                .AudioIngestSsrc = stream->GetMetadata().AudioSsrc,
            });
        if (connectResult.IsError)
        {
            spdlog::error("Failed to connect to relay target {} for channel {}: {}",
                payload.TargetHostname, payload.ChannelId, connectResult.ErrorMessage);
            return ConnectionResult
                {
                    .IsSuccess = false,
                };
        }

        stream->AddRelayClient(payload.TargetHostname, std::move(relayClient));
        
        return ConnectionResult
        {
            .IsSuccess = true,
        };
    }
    else
    {
        spdlog::info("End Stream Relay request from Orchestrator: "
            "Channel {}, Stream {}, Target: {}", payload.ChannelId, payload.StreamId,
            payload.TargetHostname);

            
        // Do we have an active stream?
        if (!streams.contains(payload.ChannelId))
        {
            spdlog::warn("Orchestrator requested to stop a relay for channel that is not streaming."
                "Target hostname: {}, Channel ID: {}", payload.TargetHostname, payload.ChannelId);
            return ConnectionResult { .IsSuccess = true };
        }
        auto& stream = streams.at(payload.ChannelId);
        if (stream->GetStreamId() != payload.StreamId)
        {
            spdlog::warn("Orchestrator requested to stop a relay for a stream that no longer exists: "
                "Channel {}, Stream {}", payload.ChannelId, payload.StreamId);
            return ConnectionResult { .IsSuccess = true };
        }
        if (!stream->StopRelay(payload.TargetHostname))
        {
            spdlog::warn("Orchestrator requested to stop non-existant relay: "
                "Channel {}, Stream {}, Target: {}", payload.ChannelId, payload.StreamId,
                payload.TargetHostname);
        }
        return ConnectionResult { .IsSuccess = true };
    }
}
#pragma endregion Private methods
