/**
 * @file FtlServer.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-14
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "FtlControlConnectionManager.h"
#include "FtlControlConnection.h"
#include "FtlStream.h"
#include "RtpPacketSink.h"
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
class FtlServer : public FtlControlConnectionManager
{
public:
    /* Public types */
    struct StartedStreamInfo {
        ftl_stream_id_t StreamId;
        std::shared_ptr<RtpPacketSink> PacketSink;
    };

    /* Callback types */
    using RequestKeyCallback = std::function<Result<std::vector<std::byte>>(ftl_channel_id_t)>;
    using StreamStartedCallback = 
        std::function<Result<StartedStreamInfo>(ftl_channel_id_t, MediaMetadata)>;
    using StreamEndedCallback = std::function<void(ftl_channel_id_t, ftl_stream_id_t)>;

    /* Constructor/Destructor */
    FtlServer(
        std::unique_ptr<ConnectionListener> ingestControlListener,
        std::unique_ptr<ConnectionCreator> mediaConnectionCreator,
        RequestKeyCallback onRequestKey,
        StreamStartedCallback onStreamStarted,
        StreamEndedCallback onStreamEnded,
        uint16_t minMediaPort = DEFAULT_MEDIA_MIN_PORT,
        uint16_t maxMediaPort = DEFAULT_MEDIA_MAX_PORT);
    virtual ~FtlServer() = default;

    /* Public functions */
    /**
     * @brief Starts listening for FTL connections on a new thread.
     */
    virtual void StartAsync();

    /**
     * @brief Stops listening for FTL connections.
     */
    virtual void Stop();

    /* FtlControlConnectionManager implementation */
    virtual void ControlConnectionStopped(FtlControlConnection* connection) override;
    virtual void ControlConnectionRequestedHmacKey(FtlControlConnection* connection,
        ftl_channel_id_t channelId) override;
    virtual void ControlConnectionRequestedMediaPort(FtlControlConnection* connection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr) override;

    /**
     * @brief Stops the stream with the specified channel ID and stream ID.
     * This will not fire the StreamEnded callback.
     */
    virtual void StopStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId);

    /**
     * @brief Retrieves stats for all active streams
     */
    virtual std::list<std::pair<std::pair<ftl_channel_id_t, ftl_stream_id_t>,
        std::pair<FtlStream::FtlStreamStats, FtlStream::FtlKeyframe>>>
        GetAllStatsAndKeyframes();

    /**
     * @brief Retrieves stats for the given stream
     */
    virtual Result<FtlStream::FtlStreamStats> GetStats(ftl_channel_id_t channelId,
        ftl_stream_id_t streamId);

private:
    /* Private types */
    struct FtlStreamRecord
    {
        FtlStreamRecord(std::shared_ptr<FtlStream> stream, uint16_t mediaPort) : 
            Stream(std::move(stream)), MediaPort(mediaPort)
        { }

        std::shared_ptr<FtlStream> Stream;
        uint16_t MediaPort;
    };
    /* Private event types */
    enum class FtlServerEventKind
    {
        Unknown = 0,
        StopStream,                 // Request to stop a specific Channel / Stream ID
        NewControlConnection,       // ConnectionListener has produced a ConnectionTransport
        ControlConnectionClosed,    // FtlControlConnection has closed
        ControlRequestHmacKey,      // Control connection requests HMAC key
        ControlHmacKeyFound,        // HMAC key has been provided for a Control connection
        ControlRequestMediaPort,    // Control connection requests media port
        TerminateControlConnection, // Terminate and remove a Control connection
        StreamIdAssigned,           // StreamStartedCallback has returned a Stream ID
        StreamStarted,              // FtlStream has started successfully
        StreamStartFailed,          // FtlStream has failed to start
        StreamClosed,               // FtlStream has closed
    };
    struct FtlServerEvent {};
    struct FtlServerStopStreamEvent : public FtlServerEvent
    {
        ftl_channel_id_t ChannelId;
        ftl_stream_id_t StreamId;
    };
    struct FtlServerNewControlConnectionEvent : public FtlServerEvent
    {
        ConnectionTransport* Connection;
    };
    struct FtlServerControlConnectionClosedEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
    };
    struct FtlServerControlRequestHmacKeyEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
        ftl_channel_id_t ChannelId;
    };
    struct FtlServerControlHmacKeyFoundEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
        std::vector<std::byte> HmacKey;
    };
    struct FtlServerControlRequestMediaPortEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
        ftl_channel_id_t ChannelId;
        MediaMetadata Metadata;
        in_addr TargetAddr;
    };
    struct FtlServerTerminateControlConnectionEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
        FtlControlConnection::FtlResponseCode ResponseCode;
    };
    struct FtlServerStreamIdAssignedEvent : public FtlServerEvent
    {
        FtlControlConnection* Connection;
        ftl_channel_id_t ChannelId;
        ftl_stream_id_t StreamId;
        MediaMetadata Metadata;
        in_addr TargetAddr;
        std::shared_ptr<RtpPacketSink> PacketSink;
    };
    struct FtlServerStreamStartedEvent : public FtlServerEvent
    {
        std::shared_ptr<FtlStream> Stream;
        ftl_channel_id_t ChannelId;
        ftl_stream_id_t StreamId;
        uint16_t MediaPort;
        in_addr TargetAddr;
    };
    struct FtlServerStreamStartFailedEvent : public FtlServerEvent
    {
        Result<void> FailureResult;
        ftl_channel_id_t ChannelId;
        ftl_stream_id_t StreamId;
        uint16_t MediaPort;
        in_addr TargetAddr;
    };
    struct FtlServerStreamClosedEvent : public FtlServerEvent
    {
        FtlStream* Stream;
    };

    /* Constants */
    static constexpr uint16_t DEFAULT_MEDIA_MIN_PORT = 9000;
    static constexpr uint16_t DEFAULT_MEDIA_MAX_PORT = 10000;
    static constexpr std::chrono::milliseconds CONNECTION_AUTH_TIMEOUT
        = std::chrono::milliseconds(5000);
    static constexpr std::chrono::milliseconds EVENT_QUEUE_WAIT_TIME
        = std::chrono::milliseconds(32);

    /* Private fields */
    // Connection managers
    const std::unique_ptr<ConnectionListener> ingestControlListener;
    const std::unique_ptr<ConnectionCreator> mediaConnectionCreator;
    // Callbacks
    const RequestKeyCallback onRequestKey;
    const StreamStartedCallback onStreamStarted;
    const StreamEndedCallback onStreamEnded;
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
    std::unordered_map<FtlControlConnection*, 
        std::pair<std::shared_ptr<FtlControlConnection>,
            std::chrono::time_point<std::chrono::steady_clock>>>
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
    void onStreamClosed(FtlStream* stream);
    void onStreamRtpPacket(ftl_channel_id_t channelId, ftl_stream_id_t streamId,
        const std::vector<std::byte>& packet);
    // Event queue listeners
    void eventStopStream(std::shared_ptr<FtlServerStopStreamEvent> event);
    void eventNewControlConnection(std::shared_ptr<FtlServerNewControlConnectionEvent> event);
    void eventControlConnectionClosed(std::shared_ptr<FtlServerControlConnectionClosedEvent> event);
    void eventControlRequestHmacKey(std::shared_ptr<FtlServerControlRequestHmacKeyEvent> event);
    void eventControlHmacKeyFound(std::shared_ptr<FtlServerControlHmacKeyFoundEvent> event);
    void eventControlRequestMediaPort(std::shared_ptr<FtlServerControlRequestMediaPortEvent> event);
    void eventTerminateControlConnection(
        std::shared_ptr<FtlServerTerminateControlConnectionEvent> event);
    void eventStreamIdAssigned(std::shared_ptr<FtlServerStreamIdAssignedEvent> event);
    void eventStreamStarted(std::shared_ptr<FtlServerStreamStartedEvent> event);
    void eventStreamStartFailed(std::shared_ptr<FtlServerStreamStartFailedEvent> event);
    void eventStreamClosed(std::shared_ptr<FtlServerStreamClosedEvent> event);
    // Callback dispatchers
    void dispatchOnStreamEnded(ftl_channel_id_t channelId, ftl_stream_id_t streamId);

    // Private template functions
    template<typename Callable>
    void dispatchAsyncCall(Callable call)
    {
        // Dispatch this call on a separate thread, but keep track of the thread so
        // we can properly join it when it has finished (or we're being destructed)
        std::promise<void> threadPromise;
        std::future<void> threadFuture = threadPromise.get_future();
        asyncProcessingThreads.emplace_back(
            std::piecewise_construct,
            std::forward_as_tuple(
                [call = std::move(call), threadPromise = std::move(threadPromise)]() mutable
                {
                    call();
                    threadPromise.set_value();
                }),
            std::forward_as_tuple(std::move(threadFuture)));
    }
};