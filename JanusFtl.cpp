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
#include "DummyCredStore.h"
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
    // TODO: Create mountpoints

    // TODO: Configurable cred store
    credStore = std::make_shared<DummyCredStore>();

    ingestServer = std::make_unique<IngestServer>(credStore);
    ingestServer->SetOnRequestMediaPort(std::bind(
        &JanusFtl::ingestMediaPortRequested,
        this,
        std::placeholders::_1));
    ingestServer->Start();

    JANUS_LOG(LOG_INFO, "FTL Plugin initialized!\n");
    return 0;
}

void JanusFtl::Destroy()
{
    JANUS_LOG(LOG_INFO, "Tearing down FTL!\n");
    // TODO: Remove all mountpoints, kill threads, sessions, etc.
    ingestServer->Stop();
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
    JsonPtr jsepPtr(message);

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

    std::shared_ptr<FtlStream> ftlStream;
    {
        std::lock_guard<std::mutex> lock(sessionFtlStreamMutex);
        if (sessionFtlStream.count(handle) <= 0)
        {
            JANUS_LOG(LOG_ERR, "FTL: No FTL stream associated with this session.");
            return;
        }
        ftlStream = sessionFtlStream[handle];
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
uint16_t JanusFtl::ingestMediaPortRequested(IngestConnection& connection)
{
    // Find a free port
    std::lock_guard<std::mutex> portsLock(ftlStreamPortsMutex);
    std::lock_guard<std::mutex> channelIdsLock(ftlStreamChannelIdsMutex);
    uint16_t targetPort = 0;
    for (uint16_t i = minMediaPort; i < maxMediaPort; ++i)
    {
        if (ftlStreamPorts.count(i) == 0)
        {
            targetPort = i;
            break;
        }
    }
    if (targetPort == 0)
    {
        throw std::runtime_error("No more free media ports could be found!");
    }

    // Spin up a new FTL stream
    uint32_t channelId = connection.GetChannelId();
    auto ftlStream = std::make_shared<FtlStream>(channelId, targetPort);
    ftlStreamPorts[targetPort] = ftlStream;
    ftlStreamChannelIds[channelId] = ftlStream;
    ftlStream->Start();
    
    // Return the port back to the ingest connection
    return targetPort;
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
    if (ftlStreamChannelIds.count(channelId) == 0)
    {
        JANUS_LOG(LOG_WARN, "FTL: Request to watch invliad channel id %d\n", channelId);
        return generateMessageErrorResponse(
            FTL_PLUGIN_ERROR_NO_SUCH_STREAM,
            "Given channel ID does not exist.");
    }
    std::shared_ptr<FtlStream> ftlStream = ftlStreamChannelIds[channelId];

    // TODO allow user to request ICE restart (new offer)

    // TODO if they're already watching, handle it

    JANUS_LOG(LOG_INFO, "FTL: Request to watch stream channel id %d\n", channelId);

    // Set this session as a viewer
    sessionFtlStream[session->GetJanusPluginSessionHandle()] = ftlStream;
    session->SetViewingStream(ftlStream);

    // Prepare JSEP paylaod
    std::string sdpOffer = generateSdpOffer(session, ftlStream);
    JsonPtr jsepPtr(json_pack("{ssss}", "type", "offer", "sdp", sdpOffer.c_str()));

    // Prepare message response
    JsonPtr eventPtr(json_object());
    json_object_set_new(eventPtr.get(), "streaming", json_string("event"));
    json_t* result = json_object();
    json_object_set_new(result, "status", json_string("preparing"));
    json_object_set_new(eventPtr.get(), "result", result);

    // Push response
    int ret = janusCore->push_event(
        session->GetJanusPluginSessionHandle(),
        pluginHandle,
        transaction,
        eventPtr.get(),
        jsepPtr.get());

    JANUS_LOG(LOG_INFO, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));

    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

janus_plugin_result* JanusFtl::handleStartMessage(
    std::shared_ptr<JanusSession> session,
    JsonPtr message,
    char* transaction)
{
    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
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
    offerStream <<  
        "m=audio 1 RTP/SAVPF 97\r\n" <<
        "c=IN IP4 1.1.1.1\r\n" <<
        "a=rtpmap:97 opus/48000/2\r\n" << // TODO: We only support Opus right now.
        "a=sendonly\r\n" <<
        "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";

    // Video media description
    offerStream <<  
        "m=video 1 RTP/SAVPF 96\r\n" <<
        "c=IN IP4 1.1.1.1\r\n" <<
        "a=rtpmap:96 H264/90000\r\n" <<       // TODO: We only support H264 right now.
        "a=rtcp-fb:96 nack\r\n" <<            // Send us NACK's
        "a=rtcp-fb:96 nack pli\r\n" <<        // Send us picture-loss-indicators
        "a=rtcp-fb:96 nack goog-remb\r\n" <<  // Send some congestion indicator thing
        "a=sendonly\r\n" <<
        "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";

    return offerStream.str();
}
#pragma endregion