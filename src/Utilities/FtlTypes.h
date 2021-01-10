/**
 * @file FtlTypes.h
 * @author Hayden McAfee(hayden@outlook.com)
 * @brief A few utility type defs for FTL/RTP data
 * @version 0.1
 * @date 2020-08-29
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C"
{
    #include <rtp.h>
}

#pragma region Typedefs for various number values
/* FTL data types */
typedef uint32_t ftl_channel_id_t;
typedef uint32_t ftl_stream_id_t;

/* RTP data types */
typedef uint8_t rtp_payload_type_t;
typedef uint16_t rtp_sequence_num_t;
typedef uint32_t rtp_ssrc_t;
typedef uint32_t rtp_timestamp_t;
#pragma endregion Typedefs for various number values

#pragma region Codecs
enum class AudioCodecKind
{
    Unsupported = 0,
    Opus
};

class SupportedAudioCodecs
{
public:
    static AudioCodecKind ParseAudioCodec(const std::string codec)
    {
        std::string lowercaseCodec = codec;
        std::transform(
            lowercaseCodec.begin(),
            lowercaseCodec.end(),
            lowercaseCodec.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (lowercaseCodec.compare("opus") == 0)
        {
            return AudioCodecKind::Opus;
        }
        return AudioCodecKind::Unsupported;
    }

    static std::string AudioCodecString(const AudioCodecKind codec)
    {
        switch (codec)
        {
        case AudioCodecKind::Opus:
            return "opus";
        case AudioCodecKind::Unsupported:
        default:
            return "";
        }
    }
};

enum class VideoCodecKind
{
    Unsupported = 0,
    H264
};

class SupportedVideoCodecs
{
public:
    static VideoCodecKind ParseVideoCodec(const std::string codec)
    {
        std::string lowercaseCodec = codec;
        std::transform(
            lowercaseCodec.begin(),
            lowercaseCodec.end(),
            lowercaseCodec.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (lowercaseCodec.compare("h264") == 0)
        {
            return VideoCodecKind::H264;
        }
        return VideoCodecKind::Unsupported;
    }

    static std::string VideoCodecString(const VideoCodecKind codec)
    {
        switch (codec)
        {
        case VideoCodecKind::H264:
            return "H264";
        case VideoCodecKind::Unsupported:
        default:
            return "";
        }
    }
};
#pragma endregion Codecs

#pragma region FTL/RTP Types
struct Keyframe
{
    Keyframe() : isCapturing(false), rtpTimestamp(0) { }
    bool isCapturing;
    uint32_t rtpTimestamp;
    std::list<std::vector<std::byte>> rtpPackets;
};

struct StreamMetadata
{
    std::string ingestServerHostname;
    uint32_t streamTimeSeconds;
    uint32_t numActiveViewers;
    uint32_t currentSourceBitrateBps;
    uint32_t numPacketsReceived;
    uint32_t numPacketsNacked;
    uint32_t numPacketsLost;
    uint16_t streamerToIngestPingMs;
    std::string streamerClientVendorName;
    std::string streamerClientVendorVersion;
    std::string videoCodec;
    std::string audioCodec;
    uint16_t videoWidth;
    uint16_t videoHeight;
};

enum class RtpRelayPacketKind
{
    Audio,
    Video
};

struct RtpRelayPacket
{
    std::vector<std::byte> rtpPacketPayload;
    RtpRelayPacketKind type;
    uint64_t channelId;
};
#pragma endregion FTL/RTP Types

#pragma region Exceptions
/**
 * @brief Exception describing a failure with preview generation
 */
struct PreviewGenerationFailedException : std::runtime_error
{
    PreviewGenerationFailedException(const char* message) throw() : 
        std::runtime_error(message)
    { }
};

/**
 * @brief Exception describing a failure in communicating to the service connection
 */
struct ServiceConnectionCommunicationFailedException : std::runtime_error
{
    ServiceConnectionCommunicationFailedException(const char* message) throw() : 
        std::runtime_error(message)
    { }
};
#pragma endregion Exceptions