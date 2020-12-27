/**
 * @file FtlClient.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-14
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

extern "C"
{
    #include <utils.h>
}

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief A faster-than-light client used to connect to other janus-ftl-plugin instances.
 */
class FtlClient
{
public:
    /* Static public members */
    static constexpr uint16_t FTL_CONTROL_PORT = 8084;

    /* Public structs */
    struct ConnectMetadata
    {
        std::string VendorName;
        std::string VendorVersion;

        bool HasVideo;
        std::string VideoCodec;
        uint32_t VideoHeight;
        uint32_t VideoWidth;
        uint32_t VideoPayloadType;
        uint32_t VideoIngestSsrc;

        bool HasAudio;
        std::string AudioCodec;
        uint32_t AudioPayloadType;
        uint32_t AudioIngestSsrc;
    };

    /* Constructor/Destructor */
    FtlClient(
        std::string targetHostname,
        ftl_channel_id_t channelId,
        std::vector<std::byte> streamKey);
    
    /* Public methods */
    /**
     * @brief Starts FTL connection on a new thread.
     */
    Result<void> ConnectAsync(FtlClient::ConnectMetadata metadata);

    /**
     * @brief Stop the connection (blocks until close is complete)
     */
    void Stop();

    /**
     * @brief Set the callback to be triggered when the connection has been closed.
     */
    void SetOnClosed(std::function<void()> onClosed);

    /**
     * @brief Relays a packet from an incoming FtlStream
     */
    void RelayPacket(RtpRelayPacket packet);

private:
    /* Private structs */
    struct FtlResponse
    {
        uint16_t statusCode;
        std::string payload;
    };

    /* Private static/constexpr members */
    static constexpr int FTL_PROTOCOL_VERSION_MAJOR = 0;
    static constexpr int FTL_PROTOCOL_VERSION_MINOR = 9;

    /* Private members */
    const std::string targetHostname;
    const ftl_channel_id_t channelId;
    const std::vector<std::byte> streamKey;
    std::atomic<bool> isStopping { false }; // Set once close has been called on the sockets and we
                                            // are waiting for the connection thread to notice.
    std::atomic<bool> isStopped { false };  // Set just before the connection thread exits.
    int controlSocketHandle = 0;
    std::promise<void> connectionThreadEndedPromise;
    std::future<void> connectionThreadEndedFuture = connectionThreadEndedPromise.get_future();
    std::thread connectionThread;
    std::mutex recvResponseMutex;
    std::condition_variable recvResponseConditionVariable;
    std::queue<FtlResponse> receivedResponses;
    uint16_t assignedMediaPort = 0;
    int mediaSocketHandle = 0;
    // Callbacks
    std::function<void()> onClosed;

    /* Private methods */
    Result<void> openControlConnection();
    Result<void> authenticateControlConnection();
    Result<void> sendControlStartStream(FtlClient::ConnectMetadata metadata);
    Result<void> openMediaConnection();
    void connectionThreadBody();
    void endConnection();
    void sendControlMessage(std::string message);
    Result<FtlClient::FtlResponse> waitForResponse(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(500000));
};