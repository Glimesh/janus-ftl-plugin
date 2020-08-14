/**
 * @file JanusSession.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "JanusSession.h"

#pragma region Constructor/Destructor
JanusSession::JanusSession(janus_plugin_session* handle) : 
    handle(handle)
{ }
#pragma endregion

#pragma region Public methods
void JanusSession::RelayRtpPacket(RtpRelayPacket rtpPacket)
{
    if (rtpPacket.type == RtpRelayPacketKind::Video)
    {
        janus_rtp_header_update(rtpPacket.rtpHeader, &rtpSwitchingContext, TRUE, 0);
        janus_plugin_rtp janusRtp = 
        {
            .video = true,
            .buffer = reinterpret_cast<char*>(rtpPacket.rtpHeader),
            .length = rtpPacket.rtpHeaderLength
        };
        janus_plugin_rtp_extensions_reset(&janusRtp.extensions);
        if (handle->gateway_handle != nullptr)
        {
            janus_callbacks* gateway = reinterpret_cast<janus_callbacks*>(handle->gateway_handle);
            gateway->relay_rtp(handle, &janusRtp);
        }
    }
    else if (rtpPacket.type == RtpRelayPacketKind::Audio)
    {
        janus_rtp_header_update(rtpPacket.rtpHeader, &rtpSwitchingContext, FALSE, 0);
        janus_plugin_rtp janusRtp = 
        {
            .video = false,
            .buffer = reinterpret_cast<char*>(rtpPacket.rtpHeader),
            .length = rtpPacket.rtpHeaderLength
        };
        janus_plugin_rtp_extensions_reset(&janusRtp.extensions);
        if (handle->gateway_handle != nullptr)
        {
            janus_callbacks* gateway = reinterpret_cast<janus_callbacks*>(handle->gateway_handle);
            gateway->relay_rtp(handle, &janusRtp);
        }
    }
}
#pragma endregion

#pragma region Getters/setters
#pragma endregion