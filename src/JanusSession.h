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

#include "Utilities/FtlTypes.h"

extern "C"
{
    #include <plugins/plugin.h>
}
#include <memory>
#include <queue>
#include <condition_variable>

class JanusSession
{
public:
    /* Constructor/Destructor */
    JanusSession(janus_plugin_session* handle, janus_callbacks* janusCore);

    /* Public methods */
    void SendRtpPacket(RtpRelayPacket rtpPacket);
    void ResetRtpSwitchingContext();
    
    /* Getters/setters */
    bool GetIsStarted();
    void SetIsStarted(bool value);
    janus_plugin_session* GetJanusPluginSessionHandle();
    int64_t GetSdpSessionId();
    int64_t GetSdpVersion();

private:
    bool isStarted = false;
    bool isStopping = false;
    janus_plugin_session* handle;
    janus_callbacks* janusCore;
    janus_rtp_switching_context rtpSwitchingContext;
    int64_t sdpSessionId;
    int64_t sdpVersion;
};