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

extern "C"
{
    #include <rtp.h>
}
#include <stdint.h>
#include <string>
#include <memory>
#include <thread>

// Kind of a combo between janus_streaming_mountpoint and janus_streaming_rtp_source
class FtlStream
{
public:
    /* Constructor/Destructor */
    FtlStream(uint64_t channelId, uint16_t mediaPort);

    /* Public methods */
    void Start();
    void Stop();

private:
    /* Private members */
    uint64_t channelId; // ID of the user who is streaming
    uint16_t mediaPort; // Port that this stream is listening on
    janus_rtp_switching_context rtpSwitchingContext;
    int mediaSocketHandle;
    std::thread streamThread;

    /* Private methods */
    void startStreamThread();
};