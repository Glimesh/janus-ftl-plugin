/**
 * @file IngestConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "ServiceConnections/ServiceConnection.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

#include <functional>
#include <map>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

extern "C"
{
    #include <netinet/in.h>
}

// Forward declarations
class ConnectionTransport;

/**
 * @brief This class manages the FTL ingest connection.
 * 
 */
class IngestConnection
{
public:
    /* Constructor/Destructor */
    IngestConnection(
        std::unique_ptr<ConnectionTransport> controlConnectionTransport,
        std::shared_ptr<ServiceConnection> serviceConnection);

    /* Public methods */
    Result<void> Start();
    void Stop();
    // Getters/Setters
    std::optional<sockaddr_in> GetAddr();
    std::optional<sockaddr_in6> GetAddr6();
    ftl_channel_id_t GetChannelId();
    std::string GetVendorName();
    std::string GetVendorVersion();
    bool GetHasVideo();
    bool GetHasAudio();
    VideoCodecKind GetVideoCodec();
    uint16_t GetVideoWidth();
    uint16_t GetVideoHeight();
    AudioCodecKind GetAudioCodec();
    rtp_ssrc_t GetAudioSsrc();
    rtp_ssrc_t GetVideoSsrc();
    rtp_payload_type_t GetAudioPayloadType();
    rtp_payload_type_t GetVideoPayloadType();
    // Callbacks
    void SetOnClosed(std::function<void(void)> callback);
    void SetOnRequestMediaConnection(std::function<uint16_t(void)> callback);

private:
    /* Constants */
    static constexpr int HMAC_PAYLOAD_SIZE = 128;
    /* Private static members */
    static const std::array<char, 4> commandDelimiter;
    /* Private members */
    const std::unique_ptr<ConnectionTransport> controlConnectionTransport;
    std::stringstream commandStream;
    bool isAuthenticated = false;
    bool isStreaming = false;
    uint32_t channelId = 0;
    const std::shared_ptr<ServiceConnection> serviceConnection;
    std::thread connectionThread;
    std::vector<std::byte> hmacPayload { 0 };
    // Stream metadata
    std::string vendorName;
    std::string vendorVersion;
    bool hasVideo = false;
    bool hasAudio = false;
    VideoCodecKind videoCodec;
    AudioCodecKind audioCodec;
    uint16_t videoWidth;
    uint16_t videoHeight;
    rtp_ssrc_t audioSsrc = 0;
    rtp_ssrc_t videoSsrc = 0;
    rtp_payload_type_t audioPayloadType = 0;
    rtp_payload_type_t videoPayloadType = 0;
    // Regex patterns
    std::regex connectPattern = std::regex(R"~(CONNECT ([0-9]+) \$([0-9a-f]+))~");
    std::regex attributePattern = std::regex(R"~((.+): (.+))~");
    // Callbacks
    std::function<void(void)> onClosed;
    std::function<uint16_t(void)> onRequestMediaConnection;

    /* Private methods */
    void writeStringToControlConnection(const std::string& str);
    // ConnectionTransport callbacks
    void onConnectionTransportClosed();
    void onConnectionTransportBytesReceived(const std::vector<std::byte>& bytes);
    // Commands
    void processCommand(std::string command);
    void processHmacCommand();
    void processConnectCommand(std::string command);
    void processAttributeCommand(std::string command);
    void processDotCommand();
    void processPingCommand();
};