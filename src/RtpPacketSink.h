/**
 * @file RtpPacketSink.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-03-10
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#pragma once

#include "Rtp/RtpPacket.h"

class RtpPacketSink
{
public:
    virtual ~RtpPacketSink() {};

    /* Public methods */
    virtual void SendRtpPacket(const RtpPacket& packet) = 0;
};
