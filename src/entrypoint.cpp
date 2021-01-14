/**
 * @file janus_ftl.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @brief 
 *  This is a plugin for Janus to support ingest of streams via the FTL (Faster-Than-Light)
 *  protocol developed for the Mixer streaming platform.
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

extern "C" 
{
    #include <plugins/plugin.h>
}

#include "JanusFtl.h"
#include <memory>

#pragma region Plugin metadata
static const unsigned int FTL_PLUGIN_VERSION        = 1;
static const char*        FTL_PLUGIN_VERSION_STRING = "0.0.1";
static const char*        FTL_PLUGIN_DESCRIPTION    = "Plugin to ingest and relay FTL streams.";
static const char*        FTL_PLUGIN_NAME           = "Janus FTL Plugin";
static const char*        FTL_PLUGIN_AUTHOR         = "Hayden McAfee";
static const char*        FTL_PLUGIN_PACKAGE        = "janus.plugin.ftl";
#pragma endregion

#pragma region Plugin references
static std::unique_ptr<JanusFtl> janusFtl;
#pragma endregion

#pragma region Plugin methods
// Init/destroy
static int Init(janus_callbacks *callback, const char *config_path);
static void Destroy(void);

// Metadata
static int GetApiCompatibility(void);
static int GetVersion(void);
static const char* GetVersionString(void);
static const char* GetDescription(void);
static const char* GetName(void);
static const char* GetAuthor(void);
static const char* GetPackage(void);

// Plugin functionality
static void CreateSession(janus_plugin_session* handle, int* error);
static struct janus_plugin_result *HandleMessage(janus_plugin_session* handle, char* transaction, json_t* message, json_t* jsep);
static json_t *HandleAdminMessage(json_t* message);
static void SetupMedia(janus_plugin_session* handle);
static void IncomingRtp(janus_plugin_session* handle, janus_plugin_rtp* packet);
static void IncomingRtcp(janus_plugin_session* handle, janus_plugin_rtcp* packet);
static void DataReady(janus_plugin_session* handle);
static void HangUpMedia(janus_plugin_session* handle);
static void DestroySession(janus_plugin_session* handle, int *error);
static json_t* QuerySession(janus_plugin_session* handle);
#pragma endregion

#pragma region Plugin setup
static janus_plugin janus_ftl_plugin =
    {
        // Init/destroy
        .init = Init,
        .destroy = Destroy,

        // Metadata
        .get_api_compatibility = GetApiCompatibility,
        .get_version = GetVersion,
        .get_version_string = GetVersionString,
        .get_description = GetDescription,
        .get_name = GetName,
        .get_author = GetAuthor,
        .get_package = GetPackage,

        // Plugin functionality
        .create_session = CreateSession,
        .handle_message = HandleMessage,
        .handle_admin_message = HandleAdminMessage,
        .setup_media = SetupMedia,
        .incoming_rtp = IncomingRtp,
        .incoming_rtcp = IncomingRtcp,
        .incoming_data = nullptr,
        .data_ready = DataReady,
        .slow_link = nullptr,
        .hangup_media = HangUpMedia,
        .destroy_session = DestroySession,
        .query_session = QuerySession,
    };
#pragma endregion

#pragma region Plugin creator
extern "C" janus_plugin *create(void)
{
    auto ingestControlListener = std::make_unique<TcpConnectionListener>();
    janusFtl = std::make_unique<JanusFtl>(&janus_ftl_plugin);
    JANUS_LOG(LOG_VERB, "%s created!\n", FTL_PLUGIN_NAME);
    return &janus_ftl_plugin;
}
#pragma endregion

#pragma region Plugin init/destroy implementation
static int Init(janus_callbacks *callback, const char* config_path)
{
    return janusFtl->Init(callback, config_path);
}

static void Destroy()
{
    janusFtl->Destroy();
}
#pragma endregion

#pragma Plugin metadata implementation
static int GetApiCompatibility()
{
    return JANUS_PLUGIN_API_VERSION;
}

static int GetVersion()
{
    return FTL_PLUGIN_VERSION;
}

static const char* GetVersionString()
{
    return FTL_PLUGIN_VERSION_STRING;
}

static const char* GetDescription()
{
    return FTL_PLUGIN_DESCRIPTION;
}

static const char* GetName()
{
    return FTL_PLUGIN_NAME;
}

static const char* GetAuthor()
{
    return FTL_PLUGIN_AUTHOR;
}

static const char* GetPackage()
{
    return FTL_PLUGIN_PACKAGE;
}
#pragma endregion

#pragma region Plugin functionality implementation
static void CreateSession(janus_plugin_session* handle, int* error)
{
    return janusFtl->CreateSession(handle, error);
}

static struct janus_plugin_result* HandleMessage(
    janus_plugin_session *handle,
    char *transaction,
    json_t *message,
    json_t *jsep)
{
    return janusFtl->HandleMessage(handle, transaction, message, jsep);
}

static json_t* HandleAdminMessage(json_t* message)
{
    return janusFtl->HandleAdminMessage(message);
}

static void SetupMedia(janus_plugin_session* handle)
{
    janusFtl->SetupMedia(handle);
}

static void IncomingRtp(janus_plugin_session* handle, janus_plugin_rtp* packet)
{
    janusFtl->IncomingRtp(handle, packet);
}

static void IncomingRtcp(janus_plugin_session* handle, janus_plugin_rtcp* packet)
{
    janusFtl->IncomingRtcp(handle, packet);
}

static void DataReady(janus_plugin_session* handle)
{
    janusFtl->DataReady(handle);
}

static void HangUpMedia(janus_plugin_session* handle)
{
    janusFtl->HangUpMedia(handle);
}

void DestroySession(janus_plugin_session* handle, int* error)
{
    janusFtl->DestroySession(handle, error);
}

json_t* QuerySession(janus_plugin_session* handle)
{
    return janusFtl->QuerySession(handle);
}
#pragma endregion