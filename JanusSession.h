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
    
    /* Getters/setters */
    std::shared_ptr<FtlStream> GetFtlStream();
    void SetFtlStream(std::shared_ptr<FtlStream> ftlStream);
private:
    janus_plugin_session* handle;
    std::shared_ptr<FtlStream> ftlStream;
};