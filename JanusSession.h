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

#include "FtlStream.h"
#include "RtpRelayPacket.h"
extern "C"
{
    #include <plugins/plugin.h>
}
#include <memory>

class FtlStream; // Forward declare, circular reference

class JanusSession
{
public:
    /* Constructor/Destructor */
    JanusSession(janus_plugin_session* handle, janus_callbacks* janusCore);

    /* Public methods */
    void RelayRtpPacket(RtpRelayPacket rtpPacket);
    void ResetRtpSwitchingContext();
    
    /* Getters/setters */
    bool GetIsStarted();
    void SetIsStarted(bool value);
    janus_plugin_session* GetJanusPluginSessionHandle();
    int64_t GetSdpSessionId();
    int64_t GetSdpVersion();
    std::shared_ptr<FtlStream> GetViewingStream();
    void SetViewingStream(std::shared_ptr<FtlStream> ftlStream);

private:
    bool isStarted = false;
    janus_plugin_session* handle;
    janus_callbacks* janusCore;
    janus_rtp_switching_context rtpSwitchingContext;
    int64_t sdpSessionId;
    int64_t sdpVersion;
    std::shared_ptr<FtlStream> viewingStream;
};