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
extern "C"
{
    #include <utils.h>
}

#pragma region Constructor/Destructor
JanusSession::JanusSession(janus_plugin_session* handle, janus_callbacks* janusCore) : 
    handle(handle),
    janusCore(janusCore),
    sdpSessionId(janus_get_real_time()),
    sdpVersion(1)
{
    JANUS_LOG(LOG_INFO, "Handle: %p", handle);
}
#pragma endregion

#pragma region Public methods
void JanusSession::RelayRtpPacket(RtpRelayPacket rtpPacket)
{
    // Don't try to relay packets if we're not started yet
    if (!isStarted)
    {
        return;
    }

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
            janusCore->relay_rtp(handle, &janusRtp);
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
            janusCore->relay_rtp(handle, &janusRtp);
        }
    }
}

void JanusSession::ResetRtpSwitchingContext()
{
    janus_rtp_switching_context_reset(&rtpSwitchingContext);
}
#pragma endregion

#pragma region Getters/setters
bool JanusSession::GetIsStarted()
{
    return isStarted;
}

void JanusSession::SetIsStarted(bool value)
{
    isStarted = value;
}

janus_plugin_session* JanusSession::GetJanusPluginSessionHandle()
{
    return handle;
}

int64_t JanusSession::GetSdpSessionId()
{
    return sdpSessionId;
}

int64_t JanusSession::GetSdpVersion()
{
    return sdpVersion;
}

std::shared_ptr<FtlStream> JanusSession::GetViewingStream()
{
    return viewingStream;
}

void JanusSession::SetViewingStream(std::shared_ptr<FtlStream> ftlStream)
{
    if (viewingStream != nullptr)
    {
        viewingStream->RemoveViewer(std::shared_ptr<JanusSession>(this));
    }
    ftlStream->AddViewer(std::shared_ptr<JanusSession>(this));
    viewingStream = ftlStream;
}
#pragma endregion