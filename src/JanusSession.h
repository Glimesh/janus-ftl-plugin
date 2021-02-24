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
//#include "Utilities/FtlTypes.h"

#include <vector>

extern "C"
{
    #include <plugins/plugin.h>
}

class JanusSession
{
public:
    /* Constructor/Destructor */
    JanusSession(janus_plugin_session* handle, janus_callbacks* janusCore);

    /* Public methods */
    void SendRtpPacket(const std::vector<std::byte>& packet, const MediaMetadata& mediaMetadata);
    void ResetRtpSwitchingContext();
    
    /* Getters/setters */
    bool GetIsStarted() const;
    void SetIsStarted(bool value);
    janus_plugin_session* GetJanusPluginSessionHandle() const;
    int64_t GetSdpSessionId() const;
    int64_t GetSdpVersion() const;

private:
    bool isStarted = false;
    bool isStopping = false;
    janus_plugin_session* handle;
    janus_callbacks* janusCore;
    int64_t sdpSessionId;
    int64_t sdpVersion;
};