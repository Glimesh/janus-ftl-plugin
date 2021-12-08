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
#include "Rtp/JanusRtpPacketBuilder.h"

extern "C"
{
    #include <rtp.h>
    #include <utils.h>
}

#pragma region Constructor/Destructor
JanusSession::JanusSession(janus_plugin_session* handle, janus_callbacks* janusCore, std::optional<Configuration::PlayoutDelay> playoutDelay) : 
    handle(handle),
    janusCore(janusCore),
    sdpSessionId(janus_get_real_time()),
    sdpVersion(1),
    playoutDelay(playoutDelay)
{ }
#pragma endregion

#pragma region Public methods
void JanusSession::SendRtpPacket(const RtpPacket& packet, const MediaMetadata& mediaMetadata)
{
    if (!isStarted || handle->gateway_handle == nullptr)
    {
        return;
    }

    auto builder = JanusRtpPacketBuilder(packet.Bytes);

    #if defined(JANUS_PLAYOUT_DELAY_SUPPORT)
    if (playoutDelay) {
        // TODO don't add extension to every packet, that wastes bytes and not every viewer needs it
        builder.PlayoutDelay(playoutDelay->MinDelay(), playoutDelay->MaxDelay());
    }
    #endif

    janus_plugin_rtp janusRtp = builder.Build(mediaMetadata.VideoPayloadType);

    janusCore->relay_rtp(handle, &janusRtp);
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
