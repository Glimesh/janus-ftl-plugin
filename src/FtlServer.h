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

#include <functional>
#include <future>
#include <memory>
#include <netinet/in.h>
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
    using StreamStartedCallback = 
        std::function<Result<ftl_stream_id_t>(ftl_channel_id_t, MediaMetadata)>;
    using StreamEndedCallback = std::function<void(ftl_channel_id_t, ftl_stream_id_t)>;
    using RtpPacketCallback = FtlStream::RtpPacketCallback;

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
    void StopStream(ftl_channel_id_t channelId, ftl_stream_id_t streamId);

private:
    /* Constants */
    static constexpr uint16_t DEFAULT_MEDIA_MIN_PORT = 9000;
    static constexpr uint16_t DEFAULT_MEDIA_MAX_PORT = 10000;

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
    // Misc fields
    std::thread listenThread;
    std::unordered_map<FtlControlConnection*, std::unique_ptr<FtlControlConnection>>
        pendingControlConnections;
    std::unordered_map<FtlStream*, std::unique_ptr<FtlStream>> activeStreams;
    std::unordered_set<uint16_t> usedMediaPorts;

    /* Private functions */
    void ingestThreadBody(std::promise<void>&& readyPromise);
    Result<uint16_t> reserveMediaPort();
    // Callback handlers
    void onNewControlConnection(std::unique_ptr<ConnectionTransport> connection);
    Result<uint16_t> onControlStartMediaPort(FtlControlConnection& controlConnection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata,
        sockaddr_in targetAddr);
    void onControlConnectionClosed(FtlControlConnection& controlConnection);
    void onStreamClosed(FtlStream& stream);
};