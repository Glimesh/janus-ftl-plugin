/**
 * @file Configuration.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-28
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "Configuration.h"
#include <cstdlib>
#include <string>
#include <algorithm>

#pragma region Public methods
void Configuration::Load()
{
    // FTL_SERVICE_CONNECTION -> ServiceConnectionKind
    if (char* serviceConnectionEnv = std::getenv("FTL_SERVICE_CONNECTION"))
    {
        std::string serviceConnectionStr = std::string(serviceConnectionEnv);
        std::transform(
            serviceConnectionStr.begin(),
            serviceConnectionStr.end(),
            serviceConnectionStr.begin(),
            [](unsigned char c){ return std::tolower(c); });
        if (serviceConnectionStr.compare("dummy") == 0)
        {
            serviceConnectionKind = ServiceConnectionKind::DummyServiceConnection;
        }
        else if (serviceConnectionStr.compare("glimesh") == 0)
        {
            serviceConnectionKind = ServiceConnectionKind::GlimeshServiceConnection;
        }
    }

    // FTL_SERVICE_GLIMESH_HOSTNAME -> GlimeshServiceHostname
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_HOSTNAME"))
    {
        glimeshServiceHostname = std::string(varVal);
    }

    // FTL_SERVICE_GLIMESH_PORT -> GlimeshServicePort
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_PORT"))
    {
        glimeshServicePort = std::stoi(varVal);
    }

    // FTL_SERVICE_GLIMESH_HTTPS -> GlimeshServiceUseHttps
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_HTTPS"))
    {
        glimeshServiceUseHttps = std::stoi(varVal);
    }

    // FTL_SERVICE_GLIMESH_CLIENTID -> GlimeshServiceClientId
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_CLIENTID"))
    {
        glimeshServiceClientId = std::string(varVal);
    }

    // FTL_SERVICE_GLIMESH_CLIENTSECRET -> GlimeshServiceClientSecret
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_CLIENTSECRET"))
    {
        glimeshServiceClientSecret = std::string(varVal);
    }
}
#pragma endregion

#pragma region Configuration values
ServiceConnectionKind Configuration::GetServiceConnectionKind()
{
    return serviceConnectionKind;
}

std::string Configuration::GetGlimeshServiceHostname()
{
    return glimeshServiceHostname;
}

uint16_t Configuration::GetGlimeshServicePort()
{
    return glimeshServicePort;
}

bool Configuration::GetGlimeshServiceUseHttps()
{
    return glimeshServiceUseHttps;
}

std::string Configuration::GetGlimeshServiceClientId()
{
    return glimeshServiceClientId;
}

std::string Configuration::GetGlimeshServiceClientSecret()
{
    return glimeshServiceClientSecret;
}
#pragma endregion