#include "JanusFtl.h"
#include <jansson.h>

#pragma region Init/Destroy
int JanusFtl::Init(janus_callbacks* callback, const char* config_path)
{
    this->janusCore = callback;

    // TODO: Create mountpoints

    JANUS_LOG(LOG_INFO, "FTL initialized!\n");
    return 0;
}

void JanusFtl::Destroy()
{
    JANUS_LOG(LOG_INFO, "Tearing down FTL!\n");
    // TODO: Remove all mountpoints, kill threads, sessions, etc.
}
#pragma endregion

#pragma region Public plugin methods
void JanusFtl::CreateSession(janus_plugin_session* handle, int* error)
{
    // TODO: Create internal representation of session and associated mountpoint
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
    // TODO
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
    // TODO
}

json_t* JanusFtl::QuerySession(janus_plugin_session* handle)
{
    // TODO

    return json_object();
}
#pragma endregion