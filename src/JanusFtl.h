/**
 * @file JanusFtl.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "Configuration.h"
#include "FtlStream.h"
#include "FtlStreamStore.h"
#include "JanssonPtr.h"
#include "JanusSession.h"
#include "ServiceConnections/ServiceConnection.h"
#include "Utilities/FtlTypes.h"

extern "C"
{
    #include <plugins/plugin.h>
    #include <rtcp.h>
}

#include <FtlOrchestrationClient.h>

#include <list>
#include <memory>
#include <map>
#include <mutex>
#include <thread>

// Forward declarations
class ConnectionCreator;
class ConnectionListener;

/**
 * @brief This class handles interactions with the Janus plugin API and Janus core.
 */
class JanusFtl
{
public:
    /* Public Members */
    static const unsigned int FTL_PLUGIN_ERROR_NO_MESSAGE        = 450;
    static const unsigned int FTL_PLUGIN_ERROR_INVALID_JSON      = 451;
    static const unsigned int FTL_PLUGIN_ERROR_INVALID_REQUEST   = 452;
    static const unsigned int FTL_PLUGIN_ERROR_MISSING_ELEMENT   = 453;
    static const unsigned int FTL_PLUGIN_ERROR_NO_SUCH_STREAM    = 455;
    static const unsigned int FTL_PLUGIN_ERROR_UNKNOWN           = 470;

    /* Constructor/Destructor */
    JanusFtl(
        janus_plugin* plugin,
        std::unique_ptr<ConnectionListener> ingestControlListener,
        std::unique_ptr<ConnectionCreator> mediaConnectionCreator);

    /* Init/Destroy */
    int Init(janus_callbacks* callback, const char* config_path);
    void Destroy();

    /* Public plugin methods */
    void CreateSession(janus_plugin_session* handle, int* error);
    struct janus_plugin_result* HandleMessage(
        janus_plugin_session* handle,
        char* transaction,
        json_t* message,
        json_t* jsep);
    json_t* HandleAdminMessage(json_t* message);
    void SetupMedia(janus_plugin_session* handle);
    void IncomingRtp(janus_plugin_session* handle, janus_plugin_rtp* packet);
    void IncomingRtcp(janus_plugin_session* handle, janus_plugin_rtcp* packet);
    void DataReady(janus_plugin_session* handle);
    void HangUpMedia(janus_plugin_session* handle);
    void DestroySession(janus_plugin_session* handle, int* error);
    json_t* QuerySession(janus_plugin_session* handle);

private:
    /* Private fields */
    janus_plugin* pluginHandle;
    janus_callbacks* janusCore;
    std::unique_ptr<ConnectionListener> ingestControlListener;
    std::unique_ptr<ConnectionCreator> mediaConnectionCreator;
    std::unique_ptr<Configuration> configuration;
    std::shared_ptr<FtlConnection> orchestrationClient;
    std::shared_ptr<ServiceConnection> serviceConnection;
    std::shared_ptr<FtlStreamStore> ftlStreamStore;
    std::shared_ptr<RelayThreadPool> relayThreadPool;
    std::thread ingestListenThread;
    uint16_t minMediaPort = 9000; // TODO: Migrate to Configuration
    uint16_t maxMediaPort = 65535; // TODO: Migrate to Configuration
    std::mutex sessionsMutex;
    std::map<janus_plugin_session*, std::shared_ptr<JanusSession>> sessions;
    std::mutex portAssignmentMutex;

    /* Private methods */
    void initOrchestratorConnection();
    void initServiceConnection();
    uint16_t newIngestFtlStream(std::shared_ptr<IngestConnection> connection);
    void ftlStreamClosed(std::weak_ptr<FtlStream> weakStream);
    // Packet handling
    void handlePsfbRtcpPacket(janus_plugin_session* handle, janus_rtcp_header* packet);
    // Message handling
    janus_plugin_result* generateMessageErrorResponse(int errorCode, std::string errorMessage);
    janus_plugin_result* handleWatchMessage(std::shared_ptr<JanusSession> session, JsonPtr message, char* transaction);
    janus_plugin_result* handleStartMessage(std::shared_ptr<JanusSession> session, JsonPtr message, char* transaction);
    int sendJsep(std::shared_ptr<JanusSession> session, std::shared_ptr<FtlStream> ftlStream, char* transaction);
    std::string generateSdpOffer(std::shared_ptr<JanusSession> session, std::shared_ptr<FtlStream> ftlStream);
    // Orchestrator message handling
    void onOrchestratorConnectionClosed();
    ConnectionResult onOrchestratorIntro(ConnectionIntroPayload payload);
    ConnectionResult onOrchestratorOutro(ConnectionOutroPayload payload);
    ConnectionResult onOrchestratorStreamRelay(ConnectionRelayPayload payload);
};