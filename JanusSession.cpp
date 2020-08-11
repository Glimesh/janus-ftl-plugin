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

#pragma region Constructor/Destructor
JanusSession::JanusSession(janus_plugin_session* handle) : 
    handle(handle)
{ }
#pragma endregion

#pragma region Getters/setters
std::shared_ptr<FtlStream> JanusSession::GetFtlStream()
{
    return ftlStream;
}

void JanusSession::SetFtlStream(std::shared_ptr<FtlStream> ftlStream)
{
    this->ftlStream = ftlStream;
}
#pragma endregion