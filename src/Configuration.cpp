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

#include <algorithm>
#include <cstdlib>
#include <limits.h>
#include <sstream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

using namespace std::chrono_literals;

#pragma region Public methods
void Configuration::Load()
{
    // Get default hostname
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    myHostname = std::string(hostname);

    // Get default dummy thumbnail path
    wordexp_t expResult { 0 };
    wordexp("~/.ftl/previews", &expResult, 0);
    dummyPreviewImagePath = std::string(*(expResult.we_wordv));
    wordfree(&expResult);

    // Set default Orchestrator PSK
    orchestratorPsk = {
        std::byte(0x00), std::byte(0x01), std::byte(0x02), std::byte(0x03),
        std::byte(0x04), std::byte(0x05), std::byte(0x06), std::byte(0x07),
        std::byte(0x08), std::byte(0x09), std::byte(0x0a), std::byte(0x0b),
        std::byte(0x0c), std::byte(0x0d), std::byte(0x0e), std::byte(0x0f),
    };

    // FTL_HOSTNAME -> MyHostname
    if (char* varVal = std::getenv("FTL_HOSTNAME"))
    {
        myHostname = std::string(varVal);
    }

    // FTL_NODE_KIND -> NodeKind
    if (char* nodeKindEnv = std::getenv("FTL_NODE_KIND"))
    {
        std::string nodeKindStr = std::string(nodeKindEnv);
        std::transform(
            nodeKindStr.begin(),
            nodeKindStr.end(),
            nodeKindStr.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (nodeKindStr.compare("standalone") == 0)
        {
            nodeKind = NodeKind::Standalone;
        }
        else if (nodeKindStr.compare("ingest") == 0)
        {
            nodeKind = NodeKind::Ingest;
        }
        else if (nodeKindStr.compare("edge") == 0)
        {
            nodeKind = NodeKind::Edge;
        }
    }

    // FTL_ORCHESTRATOR_HOSTNAME -> OrchestratorHostname
    if (char* varVal = std::getenv("FTL_ORCHESTRATOR_HOSTNAME"))
    {
        orchestratorHostname = std::string(varVal);
    }

    // FTL_ORCHESTRATOR_PORT -> OrchestratorPort
    if (char* varVal = std::getenv("FTL_ORCHESTRATOR_PORT"))
    {
        orchestratorPort = std::stoul(varVal);
    }

    // FTL_ORCHESTRATOR_PSK -> OrchestratorPsk
    if (char* varVal = std::getenv("FTL_ORCHESTRATOR_PSK"))
    {
        orchestratorPsk = hexStringToByteArray(std::string(varVal));
    }

    // FTL_ORCHESTRATOR_REGION_CODE -> OrchestratorRegionCode
    if (char* varVal = std::getenv("FTL_ORCHESTRATOR_REGION_CODE"))
    {
        orchestratorRegionCode = std::string(varVal);
    }

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
        else if (serviceConnectionStr.compare("rest") == 0)
        {
            serviceConnectionKind = ServiceConnectionKind::RestServiceConnection;
        }
    }

    // FTL_SERVICE_METADATAREPORTINTERVALMS -> ServiceConnectionMetadataReportInterval
    if (char* varVal = std::getenv("FTL_SERVICE_METADATAREPORTINTERVALMS"))
    {
        serviceConnectionMetadataReportInterval = std::chrono::milliseconds(std::stoul(varVal));
    }

    // FTL_MAX_ALLOWED_BITS_PER_SECOND -> MaxAllowedBitsPerSecond
    if (char* varVal = std::getenv("FTL_MAX_ALLOWED_BITS_PER_SECOND"))
    {
        maxAllowedBitsPerSecond = std::stoul(varVal);
    }

    // FTL_ROLLING_SIZE_AVG_MS -> RollingSizeAvgMs
    if (char* varVal = std::getenv("FTL_ROLLING_SIZE_AVG_MS"))
    {
        rollingSizeAvgMs = std::stoul(varVal);
    }

    // FTL_NACK_LOST_PACKETS -> IsNackLostPacketsEnabled
    if (char* varVal = std::getenv("FTL_NACK_LOST_PACKETS"))
    {
        nackLostPackets = std::stoul(varVal);
    }

    // FTL_PLAYOUT_DELAY_MIN_MS and FTL_PLAYOUT_DELAY_MAX_MS -> PlayoutDelay
    {
        char* minVal = std::getenv("FTL_PLAYOUT_DELAY_MIN_MS");
        char* maxVal = std::getenv("FTL_PLAYOUT_DELAY_MAX_MS");

        if (minVal || maxVal)
        {
            if (!PLAYOUT_DELAY_SUPPORT)
            {
                spdlog::warn("Ignoring playout delay configuration, option janus_playout_delay_support is not enabled");
            }
            else if (minVal && maxVal)
            {
                auto min = std::chrono::milliseconds(std::stoul(minVal));
                auto max = std::chrono::milliseconds(std::stoul(maxVal));
                playoutDelay = PlayoutDelay(min, max);
            }
            else
            {
                throw InvalidConfigurationException("Both min and max playout delay values must be set together");
            }
        }
    }

    // FTL_SERVICE_DUMMY_HMAC_KEY -> DummyHmacKey
    if (char* varVal = std::getenv("FTL_SERVICE_DUMMY_HMAC_KEY"))
    {
        size_t varValLen = strlen(varVal);
        dummyHmacKey.clear();
        dummyHmacKey.reserve(varValLen);
        for (size_t i = 0; i < varValLen; ++i)
        {
            dummyHmacKey.push_back(std::byte(varVal[i]));
        }
    }

    // FTL_SERVICE_DUMMY_PREVIEWIMAGEPATH -> DummyPreviewImagePath
    if (char* varVal = std::getenv("FTL_SERVICE_DUMMY_PREVIEWIMAGEPATH"))
    {
        // wordexp expands shell expansion on a string, allowing paths such as ~/.config/...
        wordexp_t expResult { 0 };
        wordexp(varVal, &expResult, 0);
        dummyPreviewImagePath = std::string(*(expResult.we_wordv));
        wordfree(&expResult);
    }

    // FTL_SERVICE_GLIMESH_HOSTNAME -> GlimeshServiceHostname
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_HOSTNAME"))
    {
        glimeshServiceHostname = std::string(varVal);
    }

    // FTL_SERVICE_GLIMESH_PORT -> GlimeshServicePort
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_PORT"))
    {
        glimeshServicePort = std::stoul(varVal);
    }

    // FTL_SERVICE_GLIMESH_HTTPS -> GlimeshServiceUseHttps
    if (char* varVal = std::getenv("FTL_SERVICE_GLIMESH_HTTPS"))
    {
        glimeshServiceUseHttps = std::stoul(varVal);
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

    // FTL_SERVICE_REST_HOSTNAME -> RestServiceHostname
    if (char* varVal = std::getenv("FTL_SERVICE_REST_HOSTNAME"))
    {
        restServiceHostname = std::string(varVal);
    }

    // FTL_SERVICE_REST_PORT -> RestServicePort
    if (char* varVal = std::getenv("FTL_SERVICE_REST_PORT"))
    {
        restServicePort = std::stoul(varVal);
    }

    // FTL_SERVICE_REST_HTTPS -> RestServiceUseHttps
    if (char* varVal = std::getenv("FTL_SERVICE_REST_HTTPS"))
    {
        restServiceUseHttps = std::stoul(varVal);
    }

    // FTL_SERVICE_REST_PATH_BASE -> RestServicePathBase
    if (char* varVal = std::getenv("FTL_SERVICE_REST_PATH_BASE"))
    {
        restServicePathBase = std::string(varVal);
    }

    // FTL_SERVICE_REST_AUTH_TOKEN -> RestServiceAuthToken
    if (char* varVal = std::getenv("FTL_SERVICE_REST_AUTH_TOKEN"))
    {
        restServiceAuthToken = std::string(varVal);
    }
}

Configuration::PlayoutDelay::PlayoutDelay(std::chrono::milliseconds min_ms, std::chrono::milliseconds max_ms)
{
    if (min_ms < 0ms)
    {
        throw InvalidConfigurationException("Playout delay min must be greater than or equal to 0");
    }
    if (min_ms > 40950ms)
    {
        throw InvalidConfigurationException("FTL_PLAYOUT_DELAY_MIN_MS must be less than or equal to 40950ms");
    }
    if (max_ms < 0ms)
    {
        throw InvalidConfigurationException("Playout delay max must be greater than or equal to 0");
    }
    if (max_ms > 40950ms)
    {
        throw InvalidConfigurationException("FTL_PLAYOUT_DELAY_MAX_MS must be less than or equal to 40950ms");
    }
    if (min_ms > max_ms)
    {
        throw InvalidConfigurationException("FTL_PLAYOUT_DELAY_MIN_MS cannot be greater than FTL_PLAYOUT_DELAY_MAX_MS");
    }

    // Convert validated values to units of 10ms as used in the playout-delay header extension
    min = min_ms.count() / 10;
    max = max_ms.count() / 10;
}
#pragma endregion

#pragma region Configuration values
std::string Configuration::GetMyHostname()
{
    return myHostname;
}

NodeKind Configuration::GetNodeKind()
{
    return nodeKind;
}

std::string Configuration::GetOrchestratorHostname()
{
    return orchestratorHostname;
}

uint16_t Configuration::GetOrchestratorPort()
{
    return orchestratorPort;
}

std::vector<std::byte> Configuration::GetOrchestratorPsk()
{
    return orchestratorPsk;
}

std::string Configuration::GetOrchestratorRegionCode()
{
    return orchestratorRegionCode;
}

ServiceConnectionKind Configuration::GetServiceConnectionKind()
{
    return serviceConnectionKind;
}

std::vector<std::byte> Configuration::GetDummyHmacKey()
{
    return dummyHmacKey;
}

std::string Configuration::GetDummyPreviewImagePath()
{
    return dummyPreviewImagePath;
}

std::chrono::milliseconds Configuration::GetServiceConnectionMetadataReportInterval()
{
    return serviceConnectionMetadataReportInterval;
}

uint32_t Configuration::GetMaxAllowedBitsPerSecond()
{
    return maxAllowedBitsPerSecond;
}

uint32_t Configuration::GetRollingSizeAvgMs()
{
    return rollingSizeAvgMs;
}

bool Configuration::IsNackLostPacketsEnabled()
{
    return nackLostPackets;
}

std::optional<Configuration::PlayoutDelay> Configuration::GetPlayoutDelay()
{
    return playoutDelay;
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

std::string Configuration::GetRestServiceHostname()
{
    return restServiceHostname;
}

uint16_t Configuration::GetRestServicePort()
{
    return restServicePort;
}

bool Configuration::GetRestServiceUseHttps()
{
    return restServiceUseHttps;
}

std::string Configuration::GetRestServicePathBase()
{
    return restServicePathBase;
}

std::string Configuration::GetRestServiceAuthToken()
{
    return restServiceAuthToken;
}

#pragma endregion

#pragma region Private methods
std::vector<std::byte> Configuration::hexStringToByteArray(std::string hexString)
{
    std::vector<std::byte> retVal;
    std::stringstream convertStream;

    unsigned int buffer;
    unsigned int offset = 0;
    while (offset < hexString.length()) 
    {
        convertStream.clear();
        convertStream << std::hex << hexString.substr(offset, 2);
        convertStream >> std::hex >> buffer;
        retVal.push_back(static_cast<std::byte>(buffer));
        offset += 2;
    }

    return retVal;
}
#pragma endregion