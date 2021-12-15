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

    bool isVideoPacket = (packet.Header()->Type == mediaMetadata.VideoPayloadType);

    // Sadly, we can't avoid a copy here because the janus_plugin_rtp struct doesn't take a
    // const buffer. So allocate some storage to copy.
    std::vector<std::byte> buffer(packet.Bytes.begin(), packet.Bytes.end());

    janus_plugin_rtp janusRtp = 
    {
        .video = isVideoPacket,
        .buffer = reinterpret_cast<char*>(buffer.data()),
        .length = static_cast<uint16_t>(buffer.size())
    };
    janus_plugin_rtp_extensions_reset(&janusRtp.extensions);

    #if defined(JANUS_PLAYOUT_DELAY_SUPPORT)
    if (playoutDelay) {
        // TODO don't add extension to every packet, that wastes bytes and not every viewer needs it
        janusRtp.extensions.playout_delay_min = playoutDelay->MinDelay();
        janusRtp.extensions.playout_delay_max = playoutDelay->MaxDelay();
    }
    #endif

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
