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
#include "FtlClient.h"
#include "FtlServer.h"
#include "JanusSession.h"
#include "JanusStream.h"
#include "ServiceConnections/ServiceConnection.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/JanssonPtr.h"
#include "Utilities/Result.h"
#include "Utilities/Watchdog.h"
#include "VideoDecoders/VideoDecoder.h"

extern "C"
{
    #include <plugins/plugin.h>
    #include <rtcp.h>
}

#include <atomic>
#include <condition_variable>
#include <FtlOrchestrationClient.h>
#include <future>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
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
        std::unique_ptr<ConnectionCreator> mediaConnectionCreator,
        janus_callbacks* janusCallbacks,
        const char* configPath);
    ~JanusFtl();

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
    /* Private types */
    struct ActiveSession
    {
        std::optional<ftl_channel_id_t> WatchingChannelId;
        std::unique_ptr<JanusSession> Session;
    };

    /* Private fields */
    janus_plugin* pluginHandle;
    janus_callbacks* janusCore;
    std::unique_ptr<FtlServer> ftlServer;
    std::unique_ptr<Configuration> configuration;
    std::shared_ptr<FtlConnection> orchestrationClient;
    std::shared_ptr<ServiceConnection> serviceConnection;
    std::unordered_map<VideoCodecKind, std::unique_ptr<VideoDecoder>> videoDecoders;
    uint32_t maxAllowedBitsPerSecond = 0;
    std::chrono::milliseconds metadataReportInterval = std::chrono::milliseconds::min();
    std::atomic<bool> isStopping = false;
    std::thread serviceReportThread;
    std::future<void> serviceReportThreadEndedFuture;
    std::mutex threadShutdownMutex;
    std::condition_variable threadShutdownConditionVariable;
    std::unique_ptr<Watchdog> watchdog;
    // Stream/Session/Relay data
    std::shared_mutex streamDataMutex; // Covers shared access to streams and sessions
    std::unordered_map<ftl_channel_id_t, std::shared_ptr<JanusStream>> streams;
    std::unordered_map<janus_plugin_session*, ActiveSession> sessions;
    std::unordered_map<ftl_channel_id_t, std::unordered_set<JanusSession*>> pendingViewerSessions;

    /* Private methods */
    // FtlServer Callbacks
    Result<std::vector<std::byte>> ftlServerRequestKey(ftl_channel_id_t channelId);
    Result<FtlServer::StartedStreamInfo> ftlServerStreamStarted(ftl_channel_id_t channelId,
        MediaMetadata mediaMetadata);
    void ftlServerStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId);
    // Initialization
    void initVideoDecoders();
    void initOrchestratorConnection();
    void initServiceConnection();
    void initServiceReportThread();
    // Service report thread body
    void serviceReportThreadBody(std::promise<void>&& threadEndedPromise);
    // Stream handling
    void endStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
        const std::unique_lock<std::shared_mutex>& streamDataLock);
    // Packet handling
    void handlePsfbRtcpPacket(janus_plugin_session* handle, janus_rtcp_header* packet);
    // Message handling
    janus_plugin_result* generateMessageErrorResponse(int errorCode, std::string errorMessage);
    janus_plugin_result* handleWatchMessage(ActiveSession& session, JsonPtr message,
        char* transaction);
    janus_plugin_result* handleStartMessage(ActiveSession& session, JsonPtr message,
        char* transaction);
    int sendJsep(const ActiveSession& session, const JanusStream& stream, char* transaction);
    std::string generateSdpOffer(const ActiveSession& session, const JanusStream& stream);
    // Orchestrator message handling
    void onOrchestratorConnectionClosed();
    ConnectionResult onOrchestratorIntro(ConnectionIntroPayload payload);
    ConnectionResult onOrchestratorOutro(ConnectionOutroPayload payload);
    ConnectionResult onOrchestratorStreamRelay(ConnectionRelayPayload payload);
};