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

enum class RtpRelayPacketKind
{
    Audio,
    Video
};

struct RtpRelayPacket
{
    janus_rtp_header* rtpHeader;
    uint16_t rtpHeaderLength;
    RtpRelayPacketKind type;
};