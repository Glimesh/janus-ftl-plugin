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

#include "RtpRelayPacket.h"
#include "JanusSession.h"
#include "IngestConnection.h"

extern "C"
{
    #include <rtp.h>
}
#include <stdint.h>
#include <string>
#include <memory>
#include <thread>
#include <functional>
#include <list>
#include <mutex>

class JanusSession; // Forward declare, circular reference

// Kind of a combo between janus_streaming_mountpoint and janus_streaming_rtp_source
class FtlStream
{
public:
    /* Constructor/Destructor */
    FtlStream(const std::shared_ptr<IngestConnection> ingestConnection, const uint16_t mediaPort);

    /* Public methods */
    void Start();
    void Stop();
    void AddViewer(std::shared_ptr<JanusSession> viewerSession);
    void RemoveViewer(std::shared_ptr<JanusSession> viewerSession);
    void SetOnClosed(std::function<void (FtlStream&)> callback);
    
    /* Getters/Setters */
    uint64_t GetChannelId();
    uint16_t GetMediaPort();
    std::list<std::shared_ptr<JanusSession>> GetViewers();

private:
    /* Constants */
    static constexpr uint64_t MICROSECONDS_PER_SECOND        = 1000000;
    static constexpr float    MICROSECONDS_PER_MILLISECOND   = 1000.0f;
    static constexpr uint8_t  FTL_PAYLOAD_TYPE_SENDER_REPORT = 200;
    static constexpr uint8_t  FTL_PAYLOAD_TYPE_PING          = 250;

    /* Private members */
    const std::shared_ptr<IngestConnection> ingestConnection;
    const uint16_t mediaPort; // Port that this stream is listening on
    const uint8_t audioPayloadType = 0;
    const uint8_t videoPayloadType = 0;
    janus_rtp_switching_context rtpSwitchingContext;
    int mediaSocketHandle;
    std::thread streamThread;
    std::mutex viewerSessionsMutex;
    std::list<std::shared_ptr<JanusSession>> viewerSessions;
    std::function<void (FtlStream&)> onClosed;
    bool stopping = false;

    /* Private methods */
    void ingestConnectionClosed(IngestConnection& connection);
    void startStreamThread();
    void relayRtpPacket(RtpRelayPacket rtpPacket);
    void handlePing(janus_rtp_header* rtpHeader, uint16_t length);
    void handleSenderReport(janus_rtp_header* rtpHeader, uint16_t length);
};