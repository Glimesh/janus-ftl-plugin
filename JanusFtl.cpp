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
#include <jansson.h>

#pragma region Init/Destroy
int JanusFtl::Init(janus_callbacks* callback, const char* config_path)
{
    this->janusCore = callback;

    // TODO: Read configuration
    // TODO: Create mountpoints

    // TODO: Configurable cred store
    credStore = std::make_shared<DummyCredStore>();

    ingestServer = std::make_unique<IngestServer>(credStore);
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
    std::shared_ptr<JanusSession> session = std::make_shared<JanusSession>(handle);
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
    json_t* response = json_object();

    // TODO: Parse and handle the message.

    return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
}

json_t* JanusFtl::HandleAdminMessage(json_t* message)
{
    // TODO: Parse and handle the message.

    return json_object();
}

void JanusFtl::SetupMedia(janus_plugin_session* handle)
{
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
    std::shared_ptr<FtlStream> ftlStream = session->GetFtlStream();
    if (ftlStream == nullptr)
    {
        JANUS_LOG(LOG_ERR, "FTL: No FTL stream associated with this session...\n");
        return;
    }


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