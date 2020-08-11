/**
 * @file FtlStream.h
 * @author Hayden McAfee (hayden@outlook.com
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */
#pragma once

#include <rtp.h>
#include <stdint.h>
#include <string>
#include <memory>

// Kind of a combo between janus_streaming_mountpoint and janus_streaming_rtp_source
class FtlStream
{
private:
    uint64_t channelId;
    std::string name;
    std::string description;

    uint16_t port;
    janus_rtp_switching_context rtpSwitchingContext;
    int socketHandle;
};