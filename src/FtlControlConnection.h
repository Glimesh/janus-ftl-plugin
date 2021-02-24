/**
 * @file FtlControlConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-15
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "FtlStream.h"
//#include "Utilities/FtlTypes.h"
//#include "Utilities/Result.h"

#include <functional>
#include <memory>
#include <netinet/in.h>
#include <regex>
#include <sstream>

// Forward declarations
class ConnectionTransport;

/**
 * @brief Manages incoming FTL control connections
 */
class FtlControlConnection
{
public:
    /* Public types */
    using RequestKeyCallback = std::function<Result<std::vector<std::byte>>(ftl_channel_id_t)>;
    using StartMediaPortCallback = std::function<Result<uint16_t>(
        FtlControlConnection&, ftl_channel_id_t, MediaMetadata, in_addr)>;
    using ConnectionClosedCallback = std::function<void(FtlControlConnection&)>;
    enum FtlResponseCode
    {
        // See ftl-sdk/ftl_private.h
        FTL_INGEST_RESP_UNKNOWN = 0,
        FTL_INGEST_RESP_OK = 200,
        FTL_INGEST_RESP_PING = 201,
        FTL_INGEST_RESP_BAD_REQUEST = 400,
        FTL_INGEST_RESP_UNAUTHORIZED = 401,
        FTL_INGEST_RESP_OLD_VERSION = 402,
        FTL_INGEST_RESP_AUDIO_SSRC_COLLISION = 403,
        FTL_INGEST_RESP_VIDEO_SSRC_COLLISION = 404,
        FTL_INGEST_RESP_INVALID_STREAM_KEY = 405,
        FTL_INGEST_RESP_CHANNEL_IN_USE = 406,
        FTL_INGEST_RESP_REGION_UNSUPPORTED = 407,
        FTL_INGEST_RESP_NO_MEDIA_TIMEOUT = 408,
        FTL_INGEST_RESP_GAME_BLOCKED = 409,
        FTL_INGEST_RESP_SERVER_TERMINATE = 410,
        FTL_INGEST_RESP_INTERNAL_SERVER_ERROR = 500,
        FTL_INGEST_RESP_INTERNAL_MEMORY_ERROR = 900,
        FTL_INGEST_RESP_INTERNAL_COMMAND_ERROR = 901,
        FTL_INGEST_RESP_INTERNAL_SOCKET_CLOSED = 902,
        FTL_INGEST_RESP_INTERNAL_SOCKET_TIMEOUT = 903,
    };

    /* Constructor/Destructor */
    FtlControlConnection(
        std::unique_ptr<ConnectionTransport> transport,
        RequestKeyCallback onRequestKey,
        StartMediaPortCallback onStartMediaPort,
        ConnectionClosedCallback onConnectionClosed);

    /* Getters/Setters */
    ftl_channel_id_t GetChannelId();
    void SetOnConnectionClosed(ConnectionClosedCallback onConnectionClosed);

    /* Public functions */
    Result<void> StartAsync();
    void Stop(FtlResponseCode responseCode = FtlResponseCode::FTL_INGEST_RESP_SERVER_TERMINATE);

private:
    /* Constants */
    static constexpr std::array<char, 4> delimiterSequence = { '\r', '\n', '\r', '\n' };
    static constexpr int HMAC_PAYLOAD_SIZE = 128;

    /* Private fields */
    const std::unique_ptr<ConnectionTransport> transport;
    const RequestKeyCallback onRequestKey;
    const StartMediaPortCallback onStartMediaPort;
    ConnectionClosedCallback onConnectionClosed;
    bool isAuthenticated = false;
    bool isStreaming = false;
    ftl_channel_id_t channelId = 0;
    std::vector<std::byte> hmacPayload;
    MediaMetadata mediaMetadata {};
    // Command processing
    std::string commandBuffer;
    const std::regex connectPattern = std::regex(R"~(CONNECT ([0-9]+) \$([0-9a-f]+))~");
    const std::regex attributePattern = std::regex(R"~((.+): (.+))~");

    /* Private functions */
    void onTransportBytesReceived(const std::vector<std::byte>& bytes);
    void onTransportClosed();
    void writeToTransport(const std::string& str);
    void stopConnection();
    // Command processing
    void processCommand(const std::string& command);
    void processHmacCommand();
    void processConnectCommand(const std::string& command);
    void processAttributeCommand(const std::string& command);
    void processDotCommand();
    void processPingCommand();
};