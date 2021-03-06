/**
 * @file FtlServer.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-14
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "FtlControlConnection.h"
#include "FtlStream.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

#include <condition_variable>
#include <eventpp/eventqueue.h>
#include <eventpp/utilities/argumentadapter.h>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <netinet/in.h>
#include <shared_mutex>
#include <unordered_set>
#include <unordered_map>
#include <thread>

// Forward declarations
class ConnectionCreator;
class ConnectionListener;
class ConnectionTransport;

/**
 * @brief FtlServer manages ingest control and media connections, exposing the relevant stream
 * data for consumers to use.
 */
class FtlServer
{
public:
    /* Callback types */
    using RequestKeyCallback = std::function<Result<std::vector<std::byte>>(ftl_channel_id_t)>;
    using RtpPacketCallback = FtlStream::RtpPacketCallback;
    using StreamStartedCallback = 
        std::function<Result<std::pair<ftl_stream_id_t, RtpPacketCallback>>(ftl_channel_id_t, MediaMetadata)>;
    using StreamEndedCallback = std::function<void(ftl_channel_id_t, ftl_stream_id_t)>;

    /* Constructor/Destructor */
    FtlServer(
        std::unique_ptr<ConnectionListener> ingestControlListener,
        std::unique_ptr<ConnectionCreator> mediaConnectionCreator,
        RequestKeyCallback onRequestKey,
        StreamStartedCallback onStreamStarted,
        StreamEndedCallback onStreamEnded,
        RtpPacketCallback onRtpPacket,
        uint16_t minMediaPort = DEFAULT_MEDIA_MIN_PORT,
        uint16_t maxMediaPort = DEFAULT_MEDIA_MAX_PORT);
    ~FtlServer() = default;

    /* Public functions */
    /**
     * @brief Starts listening for FTL connections on a new thread.
     */
    void StartAsync();

    /**
     * @brief Stops listening for FTL connections.
     */
    void Stop();

    /**
     * @brief Stops the stream with the specified channel ID and stream ID.
     * This will not fire the StreamEnded callback.
     */
    Result<void> StopStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
        /* HACK */ bool noBlock = false);

    /**
     * @brief Retrieves stats for all active streams
     */
    std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
        std::pair<FtlStream::FtlStreamStats, FtlStream::FtlKeyframe>>>
        GetAllStatsAndKeyframes();

    /**
     * @brief Retrieves stats for the given stream
     */
    Result<FtlStream::FtlStreamStats> GetStats(ftl_channel_id_t channelId,
        ftl_stream_id_t streamId);

private:
    /* Private types */
    struct FtlStreamRecord
    {
        FtlStreamRecord(std::unique_ptr<FtlStream> stream, uint16_t mediaPort) : 
            Stream(std::move(stream)), MediaPort(mediaPort)
        { }

        std::unique_ptr<FtlStream> Stream;
        uint16_t MediaPort;
    };
    /* Private event types */
    enum class FtlServerEventKind
    {
        Unknown = 0,
        NewControlConnection,
        ControlStartMediaPort,
        StreamIdAssigned,
        ControlConnectionClosed,
        StreamClosed,
    };
    struct FtlServerEvent {};
    struct FtlServerNewControlConnectionEvent : public FtlServerEvent
    {
        ConnectionTransport* Connection;
    };
    struct FtlServerControlConnectionClosedEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
    };
    struct FtlServerControlStartMediaPortEvent : public FtlServerEvent
    {
        std::promise<Result<uint16_t>> MediaPortResultPromise;
        FtlControlConnection* Connection;
        ftl_channel_id_t ChannelId;
        MediaMetadata Metadata;
        in_addr TargetAddr;
    };
    struct FtlServerStreamIdAssignedEvent : public FtlServerEvent
    {
        std::promise<Result<uint16_t>> MediaPortResultPromise;
        FtlControlConnection* Connection;
        ftl_channel_id_t ChannelId;
        ftl_stream_id_t StreamId;
        MediaMetadata Metadata;
        in_addr TargetAddr;
        RtpPacketCallback PacketCallback;
    };
    struct FtlServerStreamClosedEvent : public FtlServerEvent
    {
        FtlStream* Stream;
    };

    /* Constants */
    static constexpr uint16_t DEFAULT_MEDIA_MIN_PORT = 9000;
    static constexpr uint16_t DEFAULT_MEDIA_MAX_PORT = 10000;
    static constexpr uint16_t CONNECTION_AUTH_TIMEOUT_MS = 5000;
    static constexpr std::chrono::milliseconds EVENT_QUEUE_WAIT_TIME = 
        std::chrono::milliseconds(32);

    /* Private fields */
    // Connection managers
    const std::unique_ptr<ConnectionListener> ingestControlListener;
    const std::unique_ptr<ConnectionCreator> mediaConnectionCreator;
    // Callbacks
    const RequestKeyCallback onRequestKey;
    const StreamStartedCallback onStreamStarted;
    const StreamEndedCallback onStreamEnded;
    const RtpPacketCallback onRtpPacket;
    // Media ports
    const uint16_t minMediaPort;
    const uint16_t maxMediaPort;
    // Event queue
    const std::jthread eventQueueThread;
    eventpp::EventQueue<FtlServerEventKind, void (std::shared_ptr<FtlServerEvent>)> eventQueue;
    std::list<std::pair<std::jthread, std::future<void>>> asyncProcessingThreads;
    // Misc fields
    bool isStopping { false };
    std::mutex stoppingMutex;
    std::condition_variable stoppingConditionVariable;
    std::thread listenThread;
    std::shared_mutex streamDataMutex;
    std::unordered_map<FtlControlConnection*, std::unique_ptr<FtlControlConnection>>
        pendingControlConnections;
    std::unordered_map<FtlStream*, FtlStreamRecord> activeStreams;
    std::unordered_set<uint16_t> usedMediaPorts;

    /* Private functions */
    void ingestThreadBody(std::promise<void>&& readyPromise);
    void eventQueueThreadBody();
    Result<uint16_t> reserveMediaPort(const std::unique_lock<std::shared_mutex>& dataLock);
    void removeStreamRecord(FtlStream* stream, const std::unique_lock<std::shared_mutex>& dataLock);
    // Callback handlers
    void onNewControlConnection(ConnectionTransport* connection);
    std::future<Result<uint16_t>> onControlStartMediaPort(FtlControlConnection* controlConnection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr);
    void onControlConnectionClosed(FtlControlConnection* controlConnection);
    void onStreamClosed(FtlStream& stream);
    void onStreamRtpPacket(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
        const std::vector<std::byte>& packet);
    // Event queue listeners
    void eventNewControlConnection(std::shared_ptr<FtlServerNewControlConnectionEvent> event);
    void eventControlStartMediaPort(std::shared_ptr<FtlServerControlStartMediaPortEvent> event);
    void eventStreamIdAssigned(std::shared_ptr<FtlServerStreamIdAssignedEvent> event);
    void eventControlConnectionClosed(std::shared_ptr<FtlServerControlConnectionClosedEvent> event);
    void eventStreamClosed(std::shared_ptr<FtlServerStreamClosedEvent> event);
    // Callback dispatchers
    void dispatchOnStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId);
};