/**
 * @file Configuration.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-28
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <cstdint>
#include <string>

enum class ServiceConnectionKind
{
    DummyServiceConnection = 0,
    GlimeshServiceConnection = 1,
};

class Configuration
{
public:
    /* Public methods */
    void Load();

    /* Configuration values */
    std::string GetMyHostname();
    ServiceConnectionKind GetServiceConnectionKind();
    uint16_t GetServiceConnectionMetadataReportIntervalMs();
    // Glimesh Service Connection Values
    std::string GetGlimeshServiceHostname();
    uint16_t GetGlimeshServicePort();
    bool GetGlimeshServiceUseHttps();
    std::string GetGlimeshServiceClientId();
    std::string GetGlimeshServiceClientSecret();

private:
    /* Backing stores */
    std::string myHostname;
    ServiceConnectionKind serviceConnectionKind = ServiceConnectionKind::DummyServiceConnection;
    uint16_t serviceConnectionMetadataReportIntervalMs = 4000;
    // Glimesh Service Connection Backing Stores
    std::string glimeshServiceHostname = "localhost";
    uint16_t glimeshServicePort = 4000;
    bool glimeshServiceUseHttps = false;
    std::string glimeshServiceClientId;
    std::string glimeshServiceClientSecret;
};