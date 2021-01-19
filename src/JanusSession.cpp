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
    #include <rtp.h>
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
void JanusSession::SendRtpPacket(const std::vector<std::byte>& packet,
    const MediaMetadata& mediaMetadata)
{
    if (!isStarted)
    {
        return;
    }
    
    // Sadly, we can't avoid a copy here because the janus_plugin_rtp struct doesn't take a
    // const buffer. So allocate some storage to copy.
    std::byte packetBuffer[1024] { std::byte(0) };
    std::copy(packet.begin(), packet.end(), packetBuffer);

    const janus_rtp_header* rtpHeader = reinterpret_cast<janus_rtp_header*>(&packetBuffer[0]);
    bool isVideoPacket = (rtpHeader->type == mediaMetadata.VideoPayloadType);
    janus_plugin_rtp janusRtp = 
    {
        .video = isVideoPacket,
        .buffer = reinterpret_cast<char*>(&packetBuffer[0]),
        .length = static_cast<uint16_t>(packet.size())
    };
    janus_plugin_rtp_extensions_reset(&janusRtp.extensions);
    if (handle->gateway_handle != nullptr)
    {
        janusCore->relay_rtp(handle, &janusRtp);
    }
}
#pragma endregion

#pragma region Getters/setters
bool JanusSession::GetIsStarted() const
{
    return isStarted;
}

void JanusSession::SetIsStarted(bool value)
{
    isStarted = value;
}

janus_plugin_session* JanusSession::GetJanusPluginSessionHandle() const
{
    return handle;
}

int64_t JanusSession::GetSdpSessionId() const
{
    return sdpSessionId;
}

int64_t JanusSession::GetSdpVersion() const
{
    return sdpVersion;
}
#pragma endregion
