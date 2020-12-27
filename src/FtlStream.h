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

extern "C"
{
    #include <rtp.h>
}
#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>
#include <thread>

class JanusSession;

// Kind of a combo between janus_streaming_mountpoint and janus_streaming_rtp_source
class FtlStream
{
public:
    /* Constructor/Destructor */
    FtlStream(
        const std::shared_ptr<IngestConnection> ingestConnection,
        const uint16_t mediaPort,
        const std::shared_ptr<RelayThreadPool> relayThreadPool,
        const std::shared_ptr<ServiceConnection> serviceConnection,
        const uint16_t metadataReportIntervalMs,
        const std::string myHostname,
        const bool nackLostPackets = true,
        const bool generatePreviews = true);

    /* Public methods */
    void Start();
    void Stop();
    void AddViewer(std::shared_ptr<JanusSession> viewerSession);
    void RemoveViewer(std::shared_ptr<JanusSession> viewerSession);
    void SetOnClosed(std::function<void (FtlStream&)> callback);
    void SendKeyframeToViewer(std::shared_ptr<JanusSession> viewerSession);
    
    /* Getters/Setters */
    ftl_channel_id_t GetChannelId();
    ftl_stream_id_t GetStreamId();
    uint16_t GetMediaPort();
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
    std::list<std::shared_ptr<JanusSession>> GetViewers();

private:
    /* Constants */
    static constexpr uint64_t            MICROSECONDS_PER_SECOND        = 1000000;
    static constexpr float               MICROSECONDS_PER_MILLISECOND   = 1000.0f;
    static constexpr rtp_payload_type_t  FTL_PAYLOAD_TYPE_SENDER_REPORT = 200;
    static constexpr rtp_payload_type_t  FTL_PAYLOAD_TYPE_PING          = 250;

    /* Private members */
    const std::shared_ptr<IngestConnection> ingestConnection;
    const uint16_t mediaPort; // Port that this stream is listening on
    const std::shared_ptr<RelayThreadPool> relayThreadPool;
    const std::shared_ptr<ServiceConnection> serviceConnection;
    const uint16_t metadataReportIntervalMs;
    const std::string myHostname;
    const bool nackLostPackets;
    const bool generatePreviews;
    ftl_stream_id_t streamId;
    janus_rtp_switching_context rtpSwitchingContext;
    int mediaSocketHandle;
    std::thread streamThread;
    std::mutex viewerSessionsMutex;
    std::list<std::shared_ptr<JanusSession>> viewerSessions;
    std::function<void (FtlStream&)> onClosed;
    bool stopping = false;
    std::map<rtp_payload_type_t, rtp_sequence_num_t> latestSequence;
    std::map<rtp_payload_type_t, std::set<rtp_sequence_num_t>> lostPackets;
    std::unique_ptr<PreviewGenerator> previewGenerator;
    // Metadata/reporting
    std::time_t streamStartTime;
    std::atomic<uint32_t> currentSourceBitrateBps {0};
    std::atomic<uint32_t> numPacketsReceived {0};
    std::atomic<uint32_t> numPacketsNacked {0};
    std::atomic<uint32_t> numPacketsLost {0};
    std::atomic<uint16_t> streamerToIngestPingMs {0};
    std::mutex streamMetadataMutex;
    std::thread streamMetadataReportingThread;
    std::mutex keyframeMutex;
    Keyframe keyframe;
    Keyframe pendingKeyframe;
    std::set<std::shared_ptr<JanusSession>> keyframeSentToViewers;
    uint32_t lastKeyframePreviewReported = 0;

    /* Private methods */
    void ingestConnectionClosed(IngestConnection& connection);
    void startStreamThread(std::promise<void>&& streamReadyPromise);
    void startStreamMetadataReportingThread();
    void processKeyframePacket(std::shared_ptr<std::vector<unsigned char>> rtpPacket);
    void markReceivedSequence(rtp_payload_type_t payloadType, rtp_sequence_num_t receivedSequence);
    void processLostPackets(sockaddr_in remoteAddr, rtp_payload_type_t payloadType, rtp_sequence_num_t currentSequence, rtp_timestamp_t currentTimestamp);
    void handlePing(janus_rtp_header* rtpHeader, uint16_t length);
    void handleSenderReport(janus_rtp_header* rtpHeader, uint16_t length);
};