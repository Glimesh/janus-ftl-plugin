/**
 * @file GlimeshServiceConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-15
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "GlimeshServiceConnection.h"
#include <httplib.h>

#pragma region Constructor/Destructor
GlimeshServiceConnection::GlimeshServiceConnection(
    std::string hostname,
    bool useHttps) : 
    hostname(hostname),
    useHttps(useHttps)
{ }
#pragma endregion

#pragma region ServiceConnection
std::string GlimeshServiceConnection::GetHmacKey(uint32_t userId)
{
    return "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
}

uint32_t GlimeshServiceConnection::CreateStream(uint32_t userId)
{
    return 1;
}

void GlimeshServiceConnection::StartStream(uint32_t streamId)
{ }

void GlimeshServiceConnection::UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata)
{ }

void GlimeshServiceConnection::EndStream(uint32_t streamId)
{ }
#pragma endregion