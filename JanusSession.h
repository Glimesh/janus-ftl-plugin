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

class JanusSession
{
public:
    /* Constructor/Destructor */
    JanusSession(janus_plugin_session* handle);

    /* Public methods */
    void RelayRtpPacket(RtpRelayPacket rtpPacket);
    
    /* Getters/setters */
private:
    janus_plugin_session* handle;
    janus_rtp_switching_context rtpSwitchingContext;
};