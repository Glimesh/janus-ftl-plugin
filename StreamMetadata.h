/**
 * @file StreamMetadata.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-15
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <cstdint>
#include <string>

struct StreamMetadata
{
    std::string ingestServerHostname;
    uint32_t streamTimeSeconds;
    uint32_t numActiveViewers;
    uint32_t currentSourceBitrateBps;
    uint32_t numPacketsReceived;
    uint32_t numPacketsNacked;
    uint32_t numPacketsLost;
    uint16_t streamerToIngestPingMs;
    std::string streamerClientVendorName;
    std::string streamerClientVendorVersion;
    std::string videoCodec;
    std::string audioCodec;
    uint16_t videoWidth;
    uint16_t videoHeight;
};