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

#include "ServiceConnection.h"
#include "AudioCodec.h"
#include "VideoCodec.h"
#include "FtlTypes.h"

#include <thread>
#include <random>
#include <regex>
#include <memory>
#include <map>
#include <functional>

extern "C"
{
    #include <netinet/in.h>
}

/**
 * @brief This class manages the FTL ingest connection.
 * 
 */
class IngestConnection
{
public:
    /* Constructor/Destructor */
    IngestConnection(
        int connectionHandle,
        sockaddr_in acceptAddress,
        std::shared_ptr<ServiceConnection> serviceConnection);

    /* Public methods */
    void Start();
    void Stop();
    // Getters/Setters
    sockaddr_in GetAcceptAddress();
    ftl_channel_id_t GetChannelId();
    bool GetHasVideo();
    bool GetHasAudio();
    VideoCodecKind GetVideoCodec();
    AudioCodecKind GetAudioCodec();
    rtp_ssrc_t GetAudioSsrc();
    rtp_ssrc_t GetVideoSsrc();
    rtp_payload_type_t GetAudioPayloadType();
    rtp_payload_type_t GetVideoPayloadType();
    // Callbacks
    void SetOnClosed(std::function<void (IngestConnection&)> callback);
    void SetOnRequestMediaConnection(std::function<uint16_t (IngestConnection&)> callback);

private:
    /* Private static members */
    static const std::array<char, 4> commandDelimiter;
    /* Private members */
    bool isAuthenticated = false;
    bool isStreaming = false;
    uint32_t channelId = 0;
    const int connectionHandle;
    const sockaddr_in acceptAddress;
    const std::shared_ptr<ServiceConnection> serviceConnection;
    std::thread connectionThread;
    std::array<uint8_t, 128> hmacPayload;
    std::default_random_engine randomEngine { std::random_device()() };
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
    std::function<void (IngestConnection&)> onClosed;
    std::function<uint16_t (IngestConnection&)> onRequestMediaConnection;

    /* Private methods */
    void startConnectionThread();
    // Commands
    void processCommand(std::string command);
    void processHmacCommand();
    void processConnectCommand(std::string command);
    void processAttributeCommand(std::string command);
    void processDotCommand();
    void processPingCommand();
    // Utility methods
    std::string byteArrayToHexString(uint8_t* byteArray, uint32_t length);
    std::vector<uint8_t> hexStringToByteArray(std::string hexString);
};