/**
 * @file JanusRtpPacketBuilder.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-12-06
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include "JanusRtpPacketBuilder.h"

#include "RtpPacket.h"

#include <assert.h>

janus_plugin_rtp JanusRtpPacketBuilder::Build(rtp_payload_type_t videoPayloadType)
{
    bool isVideoPacket = RtpPacket::GetRtpHeader(this->buffer)->Type == videoPayloadType;

    janus_plugin_rtp janusRtp =
        {
            .video = isVideoPacket,
            .buffer = reinterpret_cast<char *>(this->buffer.data()),
            .length = static_cast<uint16_t>(this->buffer.size()),
            .extensions = this->extensions,
        };

    return janusRtp;
}
