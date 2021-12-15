/**
 * @file JanusSession.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "Configuration.h"
#include "FtlStream.h"
#include "Rtp/RtpPacket.h"
#include "Utilities/FtlTypes.h"

#include <vector>

extern "C"
{
    #include <plugins/plugin.h>
}

class JanusSession
{
public:
    /* Constructor/Destructor */
    JanusSession(janus_plugin_session* handle, janus_callbacks* janusCore, std::optional<Configuration::PlayoutDelay> playoutDelay);

    /* Public methods */
    void SendRtpPacket(const RtpPacket& packet, const MediaMetadata& mediaMetadata);
    void ResetRtpSwitchingContext();
    
    /* Getters/setters */
    bool GetIsStarted() const;
    void SetIsStarted(bool value);
    janus_plugin_session* GetJanusPluginSessionHandle() const;
    int64_t GetSdpSessionId() const;
    int64_t GetSdpVersion() const;

private:
    /* Private constants */
    // Number of times to attach the playout-delay extension to outgoing packets, if the feature is
    // enabled and configured. One one packet with the extension needs to arrive for the client to
    // store the value and use it for the rest of the session, we send it a number of times in case
    // some packets are lost. Smarter ways to do this with NACKs exist, but this is good enough.
    static constexpr size_t PLAYOUT_DELAY_SEND_COUNT_TARGET = 500;
    
    /* Private member variables */
    bool isStarted = false;
    bool isStopping = false;
    janus_plugin_session* handle;
    janus_callbacks* janusCore;
    int64_t sdpSessionId;
    int64_t sdpVersion;
    std::optional<Configuration::PlayoutDelay> playoutDelay;
    size_t playoutDelaySendCount = 0;
};
