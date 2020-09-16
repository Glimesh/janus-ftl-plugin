/**
 * @file JanusFtl.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "JanusFtl.h"
#include "DummyServiceConnection.h"
#include "JanssonPtr.h"
#include <jansson.h>
extern "C"
{
    #include <apierror.h>
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

    // TODO: Read configuration

    // TODO: Configurable cred store
    serviceConnection = std::make_shared<DummyServiceConnection>();

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
    JANUS_LOG(LOG_INFO, "FTL: SetupMedia");

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
}

void JanusFtl::IncomingRtp(janus_plugin_session* handle, janus_plugin_rtp* packet)
{
    // TODO
}

void JanusFtl::IncomingRtcp(janus_plugin_session* handle, janus_plugin_rtcp* packet)
{
    // TODO
}

void JanusFtl::DataReady(janus_plugin_session* handle)
{
    // TODO
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
    auto ftlStream = std::make_shared<FtlStream>(connection, targetPort, relayThreadPool);
    ftlStream->SetOnClosed(std::bind(
        &JanusFtl::ftlStreamClosed,
        this,
        std::placeholders::_1));
    ftlStreamStore->AddStream(ftlStream);
    ftlStream->Start();

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

    ftlStreamStore->RemoveStream(stream);
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
            "a=fmtp:" << videoPayloadType << " profile-level-id=42e01f;packetization-mode=1;"
            //"a=rtcp-fb:96 nack\r\n" <<            // Send us NACK's
            //"a=rtcp-fb:96 nack pli\r\n" <<        // Send us picture-loss-indicators
            //"a=rtcp-fb:96 nack goog-remb\r\n" <<  // Send some congestion indicator thing
            "a=sendonly\r\n" <<
            "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";
    }
    return offerStream.str();
}
#pragma endregion