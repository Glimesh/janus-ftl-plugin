/**
 * @file JanusFtl.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "FtlClient.h"
#include "JanusFtl.h"
#include "Configuration.h"
#include "DummyServiceConnection.h"
#include "EdgeNodeServiceConnection.h"
#include "GlimeshServiceConnection.h"
#include "JanssonPtr.h"
#include <jansson.h>
extern "C"
{
    #include <apierror.h>
    #include <rtcp.h>
    #include <debug.h>
}

#pragma region Constructor/Destructor
JanusFtl::JanusFtl(janus_plugin* plugin) : 
    pluginHandle(plugin)
{ }
#pragma endregion

#pragma region Init/Destroy
int JanusFtl::Init(janus_callbacks* callback, const char* config_path)
{
    this->janusCore = callback;

    configuration = std::make_unique<Configuration>();
    configuration->Load();

    initOrchestratorConnection();

    initServiceConnection();

    ftlStreamStore = std::make_shared<FtlStreamStore>();

    relayThreadPool = std::make_shared<RelayThreadPool>(ftlStreamStore);
    relayThreadPool->Start();

    ingestServer = std::make_unique<IngestServer>(serviceConnection);
    ingestServer->SetOnRequestMediaConnection(std::bind(
        &JanusFtl::newIngestFtlStream,
        this,
        std::placeholders::_1));
    ingestServer->Start();

    JANUS_LOG(LOG_INFO, "FTL: Plugin initialized!\n");
    return 0;
}

void JanusFtl::Destroy()
{
    JANUS_LOG(LOG_INFO, "FTL: Tearing down FTL!\n");
    // TODO: Remove all mountpoints, kill threads, sessions, etc.
    ingestServer->Stop();
    relayThreadPool->Stop();
}
#pragma endregion

#pragma region Public plugin methods
void JanusFtl::CreateSession(janus_plugin_session* handle, int* error)
{
    std::shared_ptr<JanusSession> session = std::make_shared<JanusSession>(handle, janusCore);
    handle->plugin_handle = session.get();
    std::lock_guard<std::mutex> lock(sessionsMutex);
    sessions[handle] = session;
}

struct janus_plugin_result* JanusFtl::HandleMessage(
        janus_plugin_session* handle,
        char* transaction,
        json_t* message,
        json_t* jsep)
{
    JsonPtr messagePtr(message);
    JsonPtr jsepPtr(jsep);

    // If we're not meant to be streaming to viewers, don't acknowledge any messages.
    if (configuration->GetNodeKind() == NodeKind::Ingest)
    {
        JANUS_LOG(LOG_WARN, "FTL: Ingest service is ignoring incoming WebRTC message.\n");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_INVALID_REQUEST,
            "Server is ingest-only.");
    }

    // Look up the session
    std::shared_ptr<JanusSession> session;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
    
        if (sessions.count(handle) <= 0)
        {
            JANUS_LOG(LOG_ERR, "FTL: No sessions associated with this handle...\n");
            return generateMessageErrorResponse(
                FTL_PLUGIN_ERROR_UNKNOWN,
                "No sessions associated with this handle.");
        }

        session = sessions[handle];
    }

    // Do we have a message?
    if (messagePtr.get() == nullptr)
    {
        JANUS_LOG(LOG_ERR, "FTL: Received empty message!\n");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_NO_MESSAGE,
            "Received empty message.");
    }
    if (!json_is_object(messagePtr.get()))
    {
        JANUS_LOG(LOG_ERR, "FTL: Error parsing JSON message!\n");
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_INVALID_JSON,
            "Error parsing JSON message.");
    }
    // Should we do JANUS_VALIDATE_JSON_OBJECT here..?

    // Parse out request
    json_t *request = json_object_get(message, "request");
    const char *requestText = json_string_value(request);
    JANUS_LOG(LOG_INFO, "FTL: New request %s\n", requestText);
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
        JANUS_LOG(LOG_WARN, "FTL: Unknown request '%s'\n", requestText);
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
    JANUS_LOG(LOG_INFO, "FTL: SetupMedia\n");

    std::shared_ptr<JanusSession> session;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        if (sessions.count(handle) <= 0)
        {
            JANUS_LOG(LOG_ERR, "FTL: No session associated with this handle...\n");
            return;
        }
        session = sessions[handle];
    }

    std::shared_ptr<FtlStream> ftlStream = ftlStreamStore->GetStreamBySession(session);
    if (ftlStream == nullptr)
    {
        JANUS_LOG(LOG_ERR, "FTL: No FTL stream associated with this session.");
        return;
    }

    session->ResetRtpSwitchingContext();
    session->SetIsStarted(true);
    // BUG: Disabling this behavior for now due to #20
    // ftlStream->SendKeyframeToViewer(session);
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
            JANUS_LOG(LOG_INFO, "FTL: Got unknown RTCP packet! Type: %d\n", rtcpHeader->type);
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
}

void JanusFtl::DestroySession(janus_plugin_session* handle, int* error)
{
    std::lock_guard<std::mutex> lock(sessionsMutex);
    if (sessions.count(handle) == 0)
    {
        JANUS_LOG(LOG_ERR, "FTL: No session associated with this handle...\n");
        *error = -2;
        return;
    }

    // TODO: Mutex
    std::shared_ptr<JanusSession> session = sessions.at(handle);
    std::shared_ptr<FtlStream> viewingStream = ftlStreamStore->GetStreamBySession(session);
    if (viewingStream != nullptr)
    {
        ftlStreamStore->RemoveViewer(viewingStream, session);

        // If we're an Edge node and there are no more viewers for this channel, we can
        // un-subscribe.
        if ((configuration->GetNodeKind() == NodeKind::Edge) &&
            (viewingStream->GetViewers().size() == 0))
        {
            // Remove temporary stream key
            const auto& edgeServiceConnection = 
                std::dynamic_pointer_cast<EdgeNodeServiceConnection>(
                    serviceConnection);
            if (edgeServiceConnection == nullptr)
            {
                throw std::runtime_error(
                    "Unexpected service connection type - expected EdgeNodeServiceConnection.");
            }
            edgeServiceConnection->ClearStreamKey(viewingStream->GetChannelId());

            JANUS_LOG(LOG_INFO,
                "FTL: Last viewer for channel %d has disconnected - unsubscribing...\n",
                viewingStream->GetChannelId());
            orchestrationClient->SendChannelSubscription(ConnectionSubscriptionPayload
                {
                    .IsSubscribe = false,
                    .ChannelId = viewingStream->GetChannelId(),
                });
        }
    }

    // If this session was marked as pending for a particular channel, remove that record
    std::optional<ftl_channel_id_t> pendingChannelId = 
        ftlStreamStore->GetPendingChannelIdForSession(session);
    if (pendingChannelId.has_value())
    {
        // Remove this viewer
        ftlStreamStore->RemovePendingViewershipForSession(session);

        std::set<std::shared_ptr<JanusSession>> outstandingPendingViewers = 
            ftlStreamStore->GetPendingViewersForChannelId(pendingChannelId.value());

        // If we're an Edge node and there are no more pending viewers for this channel, we can
        // un-subscribe.
        if ((configuration->GetNodeKind() == NodeKind::Edge) &&
            (outstandingPendingViewers.size() == 0))
        {
            // Remove temporary stream key
            const auto& edgeServiceConnection = 
                std::dynamic_pointer_cast<EdgeNodeServiceConnection>(
                    serviceConnection);
            if (edgeServiceConnection == nullptr)
            {
                throw std::runtime_error(
                    "Unexpected service connection type - expected EdgeNodeServiceConnection.");
            }
            edgeServiceConnection->ClearStreamKey(pendingChannelId.value());

            JANUS_LOG(LOG_INFO,
                "FTL: Last pending viewer for channel %d has disconnected - unsubscribing...\n",
                pendingChannelId.value());
            orchestrationClient->SendChannelSubscription(ConnectionSubscriptionPayload
                {
                    .IsSubscribe = false,
                    .ChannelId = pendingChannelId.value(),
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

uint16_t JanusFtl::newIngestFtlStream(std::shared_ptr<IngestConnection> connection)
{
    // Find a free port
    std::lock_guard<std::mutex> portAssignmentGuard(portAssignmentMutex);
    uint16_t targetPort = 0;
    for (uint16_t i = minMediaPort; i < maxMediaPort; ++i)
    {
        if (ftlStreamStore->GetStreamByMediaPort(i) == nullptr)
        {
            targetPort = i;
            break;
        }
    }
    if (targetPort == 0)
    {
        // TODO: More gracefully handle this rather than crash...
        throw std::runtime_error("No more free media ports could be found!");
    }

    // Spin up a new FTL stream
    bool nackLostPackets = ((configuration->GetNodeKind() == NodeKind::Standalone) || 
        (configuration->GetNodeKind() == NodeKind::Ingest));
    bool generatePreviews = ((configuration->GetNodeKind() == NodeKind::Standalone) || 
        (configuration->GetNodeKind() == NodeKind::Ingest));
    auto ftlStream = std::make_shared<FtlStream>(
        connection,
        targetPort,
        relayThreadPool,
        serviceConnection,
        configuration->GetServiceConnectionMetadataReportIntervalMs(),
        configuration->GetMyHostname(),
        nackLostPackets,
        generatePreviews);
    ftlStream->SetOnClosed(std::bind(
        &JanusFtl::ftlStreamClosed,
        this,
        std::placeholders::_1));
    ftlStreamStore->AddStream(ftlStream);
    ftlStream->Start();

    // If we are configured as an Ingest node, notify the Orchestrator that a stream has started.
    if ((configuration->GetNodeKind() == NodeKind::Ingest) && (orchestrationClient != nullptr))
    {
        JANUS_LOG(
            LOG_INFO,
            "FTL: Publishing channel %d / stream %d to Orchestrator...\n",
            ftlStream->GetChannelId(),
            ftlStream->GetStreamId());
        orchestrationClient->SendStreamPublish(ConnectionPublishPayload
            {
                .IsPublish = true,
                .ChannelId = ftlStream->GetChannelId(),
                .StreamId = ftlStream->GetStreamId(),
            });
    }

    // Add any pending viewers
    std::set<std::shared_ptr<JanusSession>> pendingViewers = 
        ftlStreamStore->ClearPendingViewersForChannelId(ftlStream->GetChannelId());
    for (const auto& pendingViewer : pendingViewers)
    {
        ftlStreamStore->AddViewer(ftlStream, pendingViewer);

        // Start er up!
        sendJsep(pendingViewer, ftlStream, nullptr);
    }
    
    // Return the port back to the ingest connection
    return targetPort;
}

void JanusFtl::ftlStreamClosed(FtlStream& ftlStream)
{
    // Try to find stream in stream store
    std::shared_ptr<FtlStream> stream = 
        ftlStreamStore->GetStreamByChannelId(ftlStream.GetChannelId());
    if (stream == nullptr)
    {
        stream = ftlStreamStore->GetStreamByMediaPort(ftlStream.GetMediaPort());
    }
    if (stream == nullptr)
    {
        throw std::runtime_error("Stream reporting closed could not be found.");
    }

    uint16_t channelId = stream->GetChannelId();
    
    // Remove viewers from stream and place them back into pending status
    std::list<std::shared_ptr<JanusSession>> viewers = stream->GetViewers();
    for (const std::shared_ptr<JanusSession>& viewer : viewers)
    {
        ftlStreamStore->RemoveViewer(stream, viewer);
        ftlStreamStore->AddPendingViewerForChannelId(channelId, viewer);

        // TODO: Tell the viewers that this stream has stopped
    }

    // If we are configured as an Ingest node, notify the Orchestrator that a stream has ended.
    if ((configuration->GetNodeKind() == NodeKind::Ingest) && (orchestrationClient != nullptr))
    {
        JANUS_LOG(
            LOG_INFO,
            "FTL: Unpublishing channel %d / stream %d from Orchestrator...\n",
            stream->GetChannelId(),
            stream->GetStreamId());
        orchestrationClient->SendStreamPublish(ConnectionPublishPayload
            {
                .IsPublish = false,
                .ChannelId = stream->GetChannelId(),
                .StreamId = stream->GetStreamId(),
            });
    }

    // If relays exist for this stream, stop and remove them
    std::list<FtlStreamStore::RelayStore> relays = 
        ftlStreamStore->GetRelaysForChannelId(stream->GetChannelId());
    for (const auto& relay : relays)
    {
        JANUS_LOG(
            LOG_INFO,
            "FTL: Stopping %s relay for channel %d / stream %d...\n",
            relay.TargetHost.c_str(),
            stream->GetChannelId(),
            stream->GetStreamId());
        relay.FtlClientInstance->Stop();
    }
    ftlStreamStore->ClearRelays(stream->GetChannelId());

    ftlStreamStore->RemoveStream(stream);
}

void JanusFtl::handlePsfbRtcpPacket(janus_plugin_session* handle, janus_rtcp_header* packet)
{
    switch (packet->rc)
    {
    case 1:
    {
        // BUG: Disabling this behavior for now due to #20
        // std::shared_ptr<JanusSession> session = sessions.at(handle);
        // std::shared_ptr<FtlStream> viewingStream = ftlStreamStore->GetStreamBySession(session);
        // if (viewingStream != nullptr)
        // {
        //     viewingStream->SendKeyframeToViewer(session);
        // }
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

janus_plugin_result* JanusFtl::handleWatchMessage(
    std::shared_ptr<JanusSession> session,
    JsonPtr message,
    char* transaction)
{
    json_t* channelIdJs = json_object_get(message.get(), "channelId");
    if (channelIdJs == nullptr)
    {
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_MISSING_ELEMENT,
            "Expected 'channelId' property.");
    }

    // Look up the stream associated with given channel ID
    uint32_t channelId = json_integer_value(channelIdJs);
    std::shared_ptr<FtlStream> ftlStream = ftlStreamStore->GetStreamByChannelId(channelId);
    JANUS_LOG(LOG_INFO, "FTL: Request to watch stream channel id %d.\n", channelId);

    if (ftlStream == nullptr)
    {
        // This channel doesn't have a stream running!

        // If we're an Edge node and this is a first viewer for a given channel,
        // request that this channel be relayed to us.
        if ((configuration->GetNodeKind() == NodeKind::Edge) &&
            (ftlStreamStore->GetPendingViewersForChannelId(channelId).size() == 0))
        {
            // Generate a new stream key for incoming relay of this channel
            const auto& edgeServiceConnection = 
                std::dynamic_pointer_cast<EdgeNodeServiceConnection>(
                    serviceConnection);
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
        JANUS_LOG(
            LOG_INFO,
            "FTL: No current stream for channel %d - session is pending.\n",
            channelId);
        ftlStreamStore->AddPendingViewerForChannelId(channelId, session);

        // Tell the client that we're pending an active stream
        JsonPtr eventPtr(json_object());
        json_object_set_new(eventPtr.get(), "streaming", json_string("event"));
        json_t* result = json_object();
        json_object_set_new(result, "status", json_string("pending"));
        json_object_set_new(eventPtr.get(), "result", result);

        janusCore->push_event(
            session->GetJanusPluginSessionHandle(),
            pluginHandle,
            transaction,
            eventPtr.get(),
            nullptr);

        return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
    }

    // TODO allow user to request ICE restart (new offer)

    // TODO if they're already watching, handle it

    // Set this session as a viewer
    ftlStreamStore->AddViewer(ftlStream, session);

    // Send the JSEP to initiate the media connection
    sendJsep(session, ftlStream, transaction);

    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

janus_plugin_result* JanusFtl::handleStartMessage(
    std::shared_ptr<JanusSession> session,
    JsonPtr message,
    char* transaction)
{
    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

int JanusFtl::sendJsep(
    std::shared_ptr<JanusSession> session,
    std::shared_ptr<FtlStream> ftlStream,
    char* transaction)
{
    // Prepare JSEP payload
    std::string sdpOffer = generateSdpOffer(session, ftlStream);
    JsonPtr jsepPtr(json_pack("{ssss}", "type", "offer", "sdp", sdpOffer.c_str()));

    // Prepare message response
    JsonPtr eventPtr(json_object());
    json_object_set_new(eventPtr.get(), "streaming", json_string("event"));
    json_t* result = json_object();
    json_object_set_new(result, "status", json_string("preparing"));
    json_object_set_new(eventPtr.get(), "result", result);

    // Push response
    return janusCore->push_event(
        session->GetJanusPluginSessionHandle(),
        pluginHandle,
        transaction,
        eventPtr.get(),
        jsepPtr.get());
}

std::string JanusFtl::generateSdpOffer(
    std::shared_ptr<JanusSession> session,
    std::shared_ptr<FtlStream> ftlStream)
{
    // https://tools.ietf.org/html/rfc4566

    std::stringstream offerStream;

    // Session description
    offerStream <<  
        "v=0\r\n" <<
        "o=- " << session->GetSdpSessionId() << " " << session->GetSdpVersion() << " IN IP4 127.0.0.1\r\n" <<
        "s=Channel " << ftlStream->GetChannelId() << "\r\n";

    // Audio media description
    if (ftlStream->GetHasAudio())
    {
        std::string audioPayloadType = std::to_string(ftlStream->GetAudioPayloadType());
        std::string audioCodec = SupportedAudioCodecs::AudioCodecString(ftlStream->GetAudioCodec());
        offerStream <<  
            "m=audio 1 RTP/SAVPF " << audioPayloadType << "\r\n" <<
            "c=IN IP4 1.1.1.1\r\n" <<
            "a=rtpmap:" << audioPayloadType << " " << audioCodec << "/48000/2\r\n" <<
            "a=sendonly\r\n" <<
            "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";
    }

    // Video media description
    if (ftlStream->GetHasVideo())
    {
        std::string videoPayloadType = std::to_string(ftlStream->GetVideoPayloadType());
        std::string videoCodec = SupportedVideoCodecs::VideoCodecString(ftlStream->GetVideoCodec());
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
    if (payload.IsStartRelay)
    {
        JANUS_LOG(
            LOG_INFO,
            "FTL: Start Stream Relay request from Orchestrator: Channel %d, Stream %d, Target %s\n",
            payload.ChannelId,
            payload.StreamId,
            payload.TargetHostname.c_str());

        // Do we have an active stream?
        std::shared_ptr<FtlStream> activeStream = 
            ftlStreamStore->GetStreamByChannelId(payload.ChannelId);
        if (activeStream == nullptr)
        {
            JANUS_LOG(
                LOG_ERR,
                "FTL: Orchestrator requested a relay for channel that is not streaming."
                "Target hostname: %s, Channel ID: %d\n",
                payload.TargetHostname.c_str(),
                payload.ChannelId);
            return ConnectionResult
                {
                    .IsSuccess = false,
                };
        }

        // Start the relay now!
        std::shared_ptr<FtlClient> relayClient = std::make_shared<FtlClient>(
            payload.TargetHostname,
            payload.ChannelId,
            payload.StreamKey);
        Result<void> connectResult = relayClient->ConnectAsync(FtlClient::ConnectMetadata
            {
                .VendorName = "janus-ftl-plugin",
                .VendorVersion = "0.0.0", // TODO: Versioning
                .HasVideo = activeStream->GetHasVideo(),
                .VideoCodec = 
                    SupportedVideoCodecs::VideoCodecString(activeStream->GetVideoCodec()),
                .VideoHeight = activeStream->GetVideoHeight(),
                .VideoWidth = activeStream->GetVideoWidth(),
                .VideoPayloadType = activeStream->GetVideoPayloadType(),
                .VideoIngestSsrc = activeStream->GetVideoSsrc(),
                .HasAudio = activeStream->GetHasAudio(),
                .AudioCodec = 
                    SupportedAudioCodecs::AudioCodecString(activeStream->GetAudioCodec()),
                .AudioPayloadType = activeStream->GetAudioPayloadType(),
                .AudioIngestSsrc = activeStream->GetAudioSsrc(),
            });
        if (connectResult.IsError)
        {
            JANUS_LOG(
                LOG_ERR,
                "FTL: Failed to connect to relay target %s for channel %d: %s\n",
                payload.TargetHostname.c_str(),
                payload.ChannelId,
                connectResult.ErrorMessage.c_str());
            return ConnectionResult
                {
                    .IsSuccess = false,
                };
        }

        ftlStreamStore->AddRelay(FtlStreamStore::RelayStore
            {
                .ChannelId = payload.ChannelId,
                .TargetHost = payload.TargetHostname,
                .StreamKey = payload.StreamKey,
                .FtlClientInstance = relayClient,
            });
        
        return ConnectionResult
        {
            .IsSuccess = true,
        };
    }
    else
    {
        JANUS_LOG(
            LOG_INFO,
            "FTL: End Stream Relay request from Orchestrator: Channel %d, Stream %d, Target: %s\n",
            payload.ChannelId,
            payload.StreamId,
            payload.TargetHostname.c_str());

        // Remove and stop the relay
        std::optional<FtlStreamStore::RelayStore> relay = 
            ftlStreamStore->RemoveRelay(payload.ChannelId, payload.TargetHostname);
        if (relay.has_value() == false)
        {
            JANUS_LOG(
                LOG_WARN,
                "FTL: Orchestrator requested to stop non-existant relay: "
                "Channel %d, Stream %d, Target: %s\n",
                payload.ChannelId,
                payload.StreamId,
                payload.TargetHostname.c_str());
        }
        else
        {
            // Stop the relay
            relay.value().FtlClientInstance->Stop();
        }

        return ConnectionResult
        {
            .IsSuccess = true,
        };
    }
    
}
#pragma endregion Private methods