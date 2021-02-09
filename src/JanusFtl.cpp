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
#include "PreviewGenerators/H264PreviewGenerator.h"
#include "ServiceConnections/DummyServiceConnection.h"
#include "ServiceConnections/EdgeNodeServiceConnection.h"
#include "ServiceConnections/GlimeshServiceConnection.h"
#include "Utilities/JanssonPtr.h"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
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
    std::unique_ptr<ConnectionCreator> mediaConnectionCreator) : 
    pluginHandle(plugin)
{
    ftlServer = std::make_unique<FtlServer>(std::move(ingestControlListener),
        std::move(mediaConnectionCreator),
        std::bind(&JanusFtl::ftlServerRequestKey, this, std::placeholders::_1),
        std::bind(&JanusFtl::ftlServerStreamStarted, this, std::placeholders::_1,
            std::placeholders::_2),
        std::bind(&JanusFtl::ftlServerStreamEnded, this, std::placeholders::_1,
            std::placeholders::_2),
        std::bind(&JanusFtl::ftlServerRtpPacket, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3));
}
#pragma endregion

#pragma region Init/Destroy
int JanusFtl::Init(janus_callbacks* callback, const char* config_path)
{
    this->janusCore = callback;

#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    configuration = std::make_unique<Configuration>();
    configuration->Load();
    metadataReportIntervalMs = configuration->GetServiceConnectionMetadataReportIntervalMs();

    initPreviewGenerators();

    initOrchestratorConnection();

    initServiceConnection();

    ftlServer->StartAsync();

    initServiceReportThread();

    spdlog::info("FTL plugin initialized!");
    return 0;
}

void JanusFtl::Destroy()
{
    JANUS_LOG(LOG_INFO, "FTL: Tearing down FTL!\n");
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
    auto session = std::make_unique<JanusSession>(handle, janusCore);
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
            spdlog::info("Got unknown RTCP packet! Type: {}", rtcpHeader->type);
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
    JANUS_LOG(LOG_INFO, "FTL: DataReady\n");
}

void JanusFtl::HangUpMedia(janus_plugin_session* handle)
{
    // TODO
    JANUS_LOG(LOG_WARN, "FTL: HangUpMedia called by session, but we're not handling it!\n");
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
            ActiveStream& watchingStream = streams[channelId];
            watchingStream.ViewerSessions.erase(session.Session.get());

            // If we're an Edge node and there are no more viewers for this channel, we can
            // unsubscribe.
            if ((configuration->GetNodeKind() == NodeKind::Edge) &&
                (watchingStream.ViewerSessions.size() == 0))
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

Result<ftl_stream_id_t> JanusFtl::ftlServerStreamStarted(ftl_channel_id_t channelId,
    MediaMetadata mediaMetadata)
{
    std::unique_lock lock(streamDataMutex);

    // Attempt to start the stream on the service connection
    Result<ftl_stream_id_t> startResult = serviceConnection->StartStream(channelId);
    if (startResult.IsError)
    {
        return startResult;
    }
    ftl_stream_id_t streamId = startResult.Value;

    // Stop any existing streams on this channel
    if (streams.count(channelId) > 0)
    {
        const ActiveStream& activeStream = streams[channelId];
        ftlServer->StopStream(activeStream.ChannelId, activeStream.StreamId);
        endStream(activeStream.ChannelId, activeStream.StreamId, lock);
    }

    // Insert new stream
    streams.insert_or_assign(channelId, ActiveStream
    {
        .ChannelId = channelId,
        .StreamId = streamId,
        .Metadata = mediaMetadata,
        .ViewerSessions = {},
    });

    // Move any pending viewer sessions over
    if (pendingViewerSessions.count(channelId) > 0)
    {
        for (const auto& pendingSession : pendingViewerSessions[channelId])
        {
            streams[channelId].ViewerSessions.insert(pendingSession);
            sendJsep(sessions[pendingSession->GetJanusPluginSessionHandle()], streams[channelId],
                nullptr);
        }
        pendingViewerSessions.erase(channelId);
    }
    // TODO: Notify viewer sessions

    // If we are configured as an Ingest node, notify the Orchestrator that a stream has started.
    if ((configuration->GetNodeKind() == NodeKind::Ingest) && (orchestrationClient != nullptr))
    {
        spdlog::info("FTL: Publishing channel %d / stream %d to Orchestrator...", channelId,
            streamId);
        orchestrationClient->SendStreamPublish(ConnectionPublishPayload
            {
                .IsPublish = true,
                .ChannelId = channelId,
                .StreamId = streamId,
            });
    }

    spdlog::info("New stream started. Channel {} / Stream {}.", channelId, streamId);

    return Result<ftl_stream_id_t>::Success(streamId);
}

void JanusFtl::ftlServerStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId)
{
    std::unique_lock lock(streamDataMutex);
    endStream(channelId, streamId, lock);
}

void JanusFtl::ftlServerRtpPacket(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
    const std::vector<std::byte>& packetData)
{
    std::shared_lock lock(streamDataMutex);
    if (streams.count(channelId) <= 0)
    {
        spdlog::error("Packet received for unexpected channel {}", channelId);
        return;
    }
    const ActiveStream& stream = streams[channelId];
    if (stream.StreamId != streamId)
    {
        spdlog::error("Packet received for channel {} had an unexpected stream ID: {}, expected {}",
            channelId, streamId, stream.StreamId);
        return;
    }
    for (const auto& session : stream.ViewerSessions)
    {
        session->SendRtpPacket(packetData, stream.Metadata);
    }

    if (relayClients.count(channelId) > 0)
    {
        for (const auto& relay : relayClients.at(channelId))
        {
            relay.Client->RelayPacket(packetData);
        }
    }
}

void JanusFtl::initPreviewGenerators()
{
    // H264
    previewGenerators.try_emplace(VideoCodecKind::H264, std::make_unique<H264PreviewGenerator>());
}

void JanusFtl::initOrchestratorConnection()
{
    if (configuration->GetNodeKind() != NodeKind::Standalone)
    {
        JANUS_LOG(
            LOG_INFO,
            "FTL: Connecting to Orchestration service @ %s:%d...\n",
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
        threadShutdownConditionVariable.wait_for(lock,
            std::chrono::milliseconds(metadataReportIntervalMs));
        if (isStopping)
        {
            break;
        }

        // Quickly gather data from active streams while under lock (defer reporting to avoid
        // holding up other threads)
        std::shared_lock lock(streamDataMutex);
        std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
            std::pair<FtlStream::FtlStreamStats, FtlStream::FtlKeyframe>>> statsAndKeyframes = 
                ftlServer->GetAllStatsAndKeyframes();
        std::unordered_map<ftl_channel_id_t, MediaMetadata> metadataByChannel;
        std::unordered_map<ftl_channel_id_t, uint32_t> viewersByChannel;
        for (const auto& streamInfo : statsAndKeyframes)
        {
            const ftl_channel_id_t& channelId = streamInfo.first.first;
            if (streams.count(channelId) <= 0)
            {
                continue;
            }
            metadataByChannel.try_emplace(channelId, streams.at(channelId).Metadata);
            viewersByChannel.try_emplace(channelId, streams.at(channelId).ViewerSessions.size());
        }
        lock.unlock();

        for (const auto& streamInfo : statsAndKeyframes)
        {
            const ftl_channel_id_t& channelId = streamInfo.first.first;
            const ftl_stream_id_t& streamId = streamInfo.first.second;
            const FtlStream::FtlStreamStats& stats = streamInfo.second.first;
            const FtlStream::FtlKeyframe& keyframe = streamInfo.second.second;
            if ((viewersByChannel.count(channelId) <= 0) ||
                (metadataByChannel.count(channelId) <= 0))
            {
                continue;
            }
            const MediaMetadata& mediaMetadata = metadataByChannel.at(channelId);
            const uint32_t& numActiveViewers = viewersByChannel.at(channelId);
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
                .videoWidth = mediaMetadata.VideoWidth,
                .videoHeight = mediaMetadata.VideoHeight,
            };
            serviceConnection->UpdateStreamMetadata(streamId, metadata);

            // Do we have a previewgenerator available for this stream's codec?
            if ((keyframe.Packets.size() > 0) && (previewGenerators.count(keyframe.Codec) > 0))
            {
                try
                {
                    std::vector<uint8_t> jpegBytes = 
                        previewGenerators.at(keyframe.Codec)->GenerateJpegImage(keyframe.Packets);
                    serviceConnection->SendJpegPreviewImage(streamId, jpegBytes);
                }
                catch (const PreviewGenerationFailedException& e)
                {
                    spdlog::warn("Couldn't generate preview for channel {} / stream {}: {}",
                        channelId, streamId, e.what());
                }
            }
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
    const ActiveStream& activeStream = streams[channelId];
    if (activeStream.StreamId != streamId)
    {
        spdlog::error("Stream ended from channel {} had unexpected stream id {}, expected {}",
            channelId, streamId, activeStream.StreamId);
        return;
    }

    // Reset any existing viewers to a pending state
    if (pendingViewerSessions.count(channelId) == 0)
    {
        pendingViewerSessions.insert_or_assign(channelId, std::unordered_set<JanusSession*>());
    }
    pendingViewerSessions[channelId].insert(activeStream.ViewerSessions.begin(),
        activeStream.ViewerSessions.end());
    // TODO: Tell viewers stream is offline.

    // If we are configured as an Ingest node, notify the Orchestrator that a stream has ended.
    if ((configuration->GetNodeKind() == NodeKind::Ingest) && (orchestrationClient != nullptr))
    {
        spdlog::info("Unpublishing channel {} / stream {} from Orchestrator",
            activeStream.ChannelId, activeStream.StreamId);
        orchestrationClient->SendStreamPublish(ConnectionPublishPayload
            {
                .IsPublish = false,
                .ChannelId = activeStream.ChannelId,
                .StreamId = activeStream.StreamId,
            });
    }

    // If relays exist for this stream, stop them
    if (relayClients.count(channelId) > 0)
    {
        for (const auto& relay : relayClients.at(channelId))
        {
            spdlog::info("Stopping relay for channel {} / stream {} -> {}...", activeStream.ChannelId,
                activeStream.StreamId, relay.TargetHostname);
            relay.Client->Stop();
        }
        relayClients.erase(channelId);
    }

    spdlog::info("Stream ended. Channel {} / stream {}", activeStream.ChannelId,
        activeStream.StreamId);

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
            JANUS_LOG(LOG_INFO,
                "FTL: First viewer for channel %d - subscribing...\n",
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
    ActiveStream& stream = streams[channelId];

    // TODO allow user to request ICE restart (new offer)

    // TODO if they're already watching, handle it

    // Set this session as a viewer
    stream.ViewerSessions.insert(session.Session.get());

    // Send the JSEP to initiate the media connection
    sendJsep(session, stream, transaction);

    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

janus_plugin_result* JanusFtl::handleStartMessage(ActiveSession& session, JsonPtr message,
    char* transaction)
{
    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

int JanusFtl::sendJsep(const ActiveSession& session, const ActiveStream& stream, char* transaction)
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

std::string JanusFtl::generateSdpOffer(const ActiveSession& session, const ActiveStream& stream)
{
    // https://tools.ietf.org/html/rfc4566

    std::stringstream offerStream;

    // Session description
    offerStream <<  
        "v=0\r\n" <<
        "o=- " << session.Session->GetSdpSessionId() << " " << session.Session->GetSdpVersion() << " IN IP4 127.0.0.1\r\n" <<
        "s=Channel " << stream.ChannelId << "\r\n";

    // Audio media description
    if (stream.Metadata.HasAudio)
    {
        std::string audioPayloadType = std::to_string(stream.Metadata.AudioPayloadType);
        std::string audioCodec = 
            SupportedAudioCodecs::AudioCodecString(stream.Metadata.AudioCodec);
        offerStream <<  
            "m=audio 1 RTP/SAVPF " << audioPayloadType << "\r\n" <<
            "c=IN IP4 1.1.1.1\r\n" <<
            "a=rtpmap:" << audioPayloadType << " " << audioCodec << "/48000/2\r\n" <<
            "a=sendonly\r\n" <<
            "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";
    }

    // Video media description
    if (stream.Metadata.HasVideo)
    {
        std::string videoPayloadType = std::to_string(stream.Metadata.VideoPayloadType);
        std::string videoCodec = 
            SupportedVideoCodecs::VideoCodecString(stream.Metadata.VideoCodec);
        offerStream <<  
            "m=video 1 RTP/SAVPF " << videoPayloadType << "\r\n" <<
            "c=IN IP4 1.1.1.1\r\n" <<
            "a=rtpmap:" << videoPayloadType << " " << videoCodec << "/90000\r\n" <<
            "a=fmtp:" << videoPayloadType << " profile-level-id=42e01f;packetization-mode=1;\r\n"
            "a=rtcp-fb:" << videoPayloadType << " nack\r\n" <<            // Send us NACK's
            "a=rtcp-fb:" << videoPayloadType << " nack pli\r\n" <<        // Send us picture-loss-indicators
            // "a=rtcp-fb:96 nack goog-remb\r\n" <<  // Send some congestion indicator thing
            "a=sendonly\r\n" <<
            "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";
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
    JANUS_LOG(LOG_INFO, "FTL: Received Intro from Orchestrator.\n");
    return ConnectionResult
    {
        .IsSuccess = true,
    };
}

ConnectionResult JanusFtl::onOrchestratorOutro(ConnectionOutroPayload payload)
{
    JANUS_LOG(
        LOG_INFO,
        "FTL: Received Outro from Orchestrator: %s\n",
        payload.DisconnectReason.c_str());

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
        if (streams.count(payload.ChannelId) <= 0)
        {
            spdlog::error("Orchestrator requested a relay for channel that is not streaming."
                "Target hostname: {}, Channel ID: {}", payload.TargetHostname, payload.ChannelId);
            return ConnectionResult
                {
                    .IsSuccess = false,
                };
        }
        ActiveStream& activeStream = streams[payload.ChannelId];

        // Start the relay now!
        auto relayClient = std::make_unique<FtlClient>(payload.TargetHostname, payload.ChannelId,
            payload.StreamKey);
        Result<void> connectResult = relayClient->ConnectAsync(FtlClient::ConnectMetadata
            {
                .VendorName = "janus-ftl-plugin",
                .VendorVersion = "0.0.0", // TODO: Versioning
                .HasVideo = activeStream.Metadata.HasVideo,
                .VideoCodec = SupportedVideoCodecs::VideoCodecString(
                    activeStream.Metadata.VideoCodec),
                .VideoHeight = activeStream.Metadata.VideoHeight,
                .VideoWidth = activeStream.Metadata.VideoWidth,
                .VideoPayloadType = activeStream.Metadata.VideoPayloadType,
                .VideoIngestSsrc = activeStream.Metadata.VideoSsrc,
                .HasAudio = activeStream.Metadata.HasAudio,
                .AudioCodec = SupportedAudioCodecs::AudioCodecString(
                    activeStream.Metadata.AudioCodec),
                .AudioPayloadType = activeStream.Metadata.AudioPayloadType,
                .AudioIngestSsrc = activeStream.Metadata.AudioSsrc,
            });
        if (connectResult.IsError)
        {
            spdlog::error("Failed to connect to relay target %s for channel {}: {}",
                payload.TargetHostname, payload.ChannelId, connectResult.ErrorMessage);
            return ConnectionResult
                {
                    .IsSuccess = false,
                };
        }

        if (relayClients.count(payload.ChannelId) <= 0)
        {
            relayClients.insert_or_assign(payload.ChannelId, std::list<ActiveRelay>());
        }
        relayClients.at(payload.ChannelId).emplace_back(payload.ChannelId, payload.TargetHostname,
            std::move(relayClient));
        
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

        // Remove and stop matching relays
        int numRelaysRemoved = 0;
        if (relayClients.count(payload.ChannelId) > 0)
        {
            for (auto it = relayClients.at(payload.ChannelId).begin();
                it != relayClients.at(payload.ChannelId).end();)
            {
                ActiveRelay& relay = *it;
                if ((relay.ChannelId == payload.ChannelId) &&
                    (relay.TargetHostname == payload.TargetHostname))
                {
                    relay.Client->Stop();
                    it = relayClients.at(payload.ChannelId).erase(it);
                    ++numRelaysRemoved;
                }
                else
                {
                    ++it;
                }
            }
        }

        if (numRelaysRemoved == 0)
        {
            spdlog::warn("Orchestrator requested to stop non-existant relay: "
                "Channel {}, Stream {}, Target: {}", payload.ChannelId, payload.StreamId,
                payload.TargetHostname);
        }

        return ConnectionResult
        {
            .IsSuccess = true,
        };
    }
}
#pragma endregion Private methods
