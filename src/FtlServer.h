/**
 * @file FtlServer.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-14
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include <functional>
#include <future>
#include <memory>
#include <thread>

#include "Utilities/FtlTypes.h"

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
    using StreamStartedCallback = std::function<ftl_stream_id_t(ftl_channel_id_t channelId)>;
    using StreamEndedCallback = std::function<void(ftl_channel_id_t, ftl_stream_id_t)>;
    using RtpPacketCallback = std::function<void(
        ftl_channel_id_t, ftl_stream_id_t, const std::vector<std::byte>& packetData)>;

    /* Constructor/Destructor */
    FtlServer(
        std::unique_ptr<ConnectionListener> ingestControlListener,
        std::unique_ptr<ConnectionCreator> mediaConnectionCreator);

    /* Public functions */
    /**
     * @brief Starts listening for FTL connections on a new thread.
     */
    void StartAsync();

    /**
     * @brief Stops listening for FTL connections.
     */
    void Stop();

    /* Callback setters */
    /**
     * @brief Sets callback that is fired when a new stream has started for a given channel.
     * The callback expects that the callee assigns a stream ID to the new stream via the
     * return value.
     */
    void SetOnStreamStarted(StreamStartedCallback onStreamStarted);

    /**
     * @brief Sets callback that is fired when an existing stream has ended.
     */
    void SetOnStreamEnded(StreamEndedCallback onStreamEnded);

    /**
     * @brief Sets callback that is fired when a new RTP packet is received for a running stream.
     */
    void SetOnRtpPacket(RtpPacketCallback onRtpPacket);

private:
    /* Private fields */
    const std::unique_ptr<ConnectionListener> ingestControlListener;
    const std::unique_ptr<ConnectionCreator> mediaConnectionCreator;
    std::thread listenThread;
    // Callbacks
    StreamStartedCallback onStreamStarted;
    StreamEndedCallback onStreamEnded;
    RtpPacketCallback onRtpPacket;

    /* Private functions */
    void ingestThreadBody(std::promise<void>&& readyPromise);
    // Callback handlers
    void onNewControlConnection(std::unique_ptr<ConnectionTransport> connection);
};