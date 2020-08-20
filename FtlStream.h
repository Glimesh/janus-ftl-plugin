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
    /* Private members */
    const std::shared_ptr<IngestConnection> ingestConnection;
    const uint16_t mediaPort; // Port that this stream is listening on
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
};