/**
 * @file RtpRelayPacket.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-14
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

extern "C"
{
    #include <rtp.h>
}
#include <memory>
#include <vector>

enum class RtpRelayPacketKind
{
    Audio,
    Video
};

struct RtpRelayPacket
{
    std::shared_ptr<std::vector<unsigned char>> rtpPacketPayload;
    RtpRelayPacketKind type;
    uint64_t channelId;
};