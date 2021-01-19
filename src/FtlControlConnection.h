/**
 * @file FtlControlConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-15
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "FtlStream.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

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
        FtlControlConnection&, ftl_channel_id_t, MediaMetadata, sockaddr_in)>;
    using ConnectionClosedCallback = std::function<void(FtlControlConnection&)>;

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
    void Stop();

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