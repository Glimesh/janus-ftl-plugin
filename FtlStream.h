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
    FtlStream(uint64_t channelId, uint16_t mediaPort);

    /* Public methods */
    void Start();
    void Stop();
    void AddViewer(std::shared_ptr<JanusSession> viewerSession);
    void RemoveViewer(std::shared_ptr<JanusSession> viewerSession);
    
    /* Getters/Setters */
    uint64_t GetChannelId();

private:
    /* Private members */
    uint64_t channelId; // ID of the user who is streaming
    uint16_t mediaPort; // Port that this stream is listening on
    janus_rtp_switching_context rtpSwitchingContext;
    int mediaSocketHandle;
    std::thread streamThread;
    std::mutex viewerSessionsMutex;
    std::list<std::shared_ptr<JanusSession>> viewerSessions;

    /* Private methods */
    void startStreamThread();
    void relayRtpPacket(RtpRelayPacket rtpPacket);
};