/**
 * @file FtlStream.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */
#pragma once

#include "IngestConnection.h"
#include "PreviewGenerators/H264PreviewGenerator.h"
#include "RelayThreadPool.h"
#include "Utilities/FtlTypes.h"
#include "Utilities/Result.h"

extern "C"
{
    #include <rtp.h>
}
#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>
#include <thread>

class ConnectionTransport;
class FtlControlConnection;

/**
 * @brief Manages the FTL media stream, accepting incoming RTP packets.
 */
class FtlStream
{
public:
    /* Public types */
    struct MediaMetadata
    {
        std::string VendorName;
        std::string VendorVersion;
        bool HasVideo;
        bool HasAudio;
        VideoCodecKind VideoCodec;
        AudioCodecKind AudioCodec;
        uint16_t VideoWidth;
        uint16_t VideoHeight;
        rtp_ssrc_t VideoSsrc;
        rtp_ssrc_t AudioSsrc;
        rtp_payload_type_t VideoPayloadType;
        rtp_payload_type_t AudioPayloadType;
    };
    using ClosedCallback = std::function<void(FtlStream&)>;
    using RtpPacketCallback = FtlServer::RtpPacketCallback;

    /* Constructor/Destructor */
    FtlStream(
        std::unique_ptr<FtlControlConnection> controlConnection,
        std::unique_ptr<ConnectionTransport> mediaTransport,
        const MediaMetadata mediaMetadata,
        const ftl_stream_id_t streamId,
        const ClosedCallback onClosed,
        const RtpPacketCallback onRtpPacket,
        const bool nackLostPackets = true);

    /* Public methods */
    Result<void> StartAsync();
    void Stop();

    /* Getters/Setters */
    ftl_channel_id_t GetChannelId();
    ftl_stream_id_t GetStreamId();

private:
    /* Constants */
    static constexpr uint64_t            MICROSECONDS_PER_SECOND        = 1000000;
    static constexpr float               MICROSECONDS_PER_MILLISECOND   = 1000.0f;
    static constexpr rtp_payload_type_t  FTL_PAYLOAD_TYPE_SENDER_REPORT = 200;
    static constexpr rtp_payload_type_t  FTL_PAYLOAD_TYPE_PING          = 250;

    /* Private members */
    const std::unique_ptr<FtlControlConnection> controlConnection;
    const std::unique_ptr<ConnectionTransport> mediaTransport;
    const MediaMetadata mediaMetadata;
    const ftl_stream_id_t streamId;
    const ClosedCallback onClosed;
    const RtpPacketCallback onRtpPacket;
    const bool nackLostPackets;
    bool stopping = false;

    /* Private methods */
    void controlConnectionClosed(FtlControlConnection& connection);
    void mediaBytesReceived(const std::vector<std::byte>& bytes);
    void mediaConnectionClosed();
    void handlePing(const std::vector<std::byte>& rtpPacket);
    void handleSenderReport(const std::vector<std::byte>& rtpPacket);
};