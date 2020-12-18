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
#include <vector>

enum class NodeKind
{
    Standalone = 0,
    Ingest = 1,
    // Relay = 2,
    Edge = 3,
};

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
    NodeKind GetNodeKind();
    std::string GetOrchestratorHostname();
    uint16_t GetOrchestratorPort();
    std::vector<std::byte> GetOrchestratorPsk();
    std::string GetOrchestratorRegionCode();
    ServiceConnectionKind GetServiceConnectionKind();
    uint16_t GetServiceConnectionMetadataReportIntervalMs();

    // Dummy Service Connection Values
    std::string GetDummyHmacKey();
    std::string GetDummyPreviewImagePath();

    // Glimesh Service Connection Values
    std::string GetGlimeshServiceHostname();
    uint16_t GetGlimeshServicePort();
    bool GetGlimeshServiceUseHttps();
    std::string GetGlimeshServiceClientId();
    std::string GetGlimeshServiceClientSecret();

private:
    /* Backing stores */
    std::string myHostname;
    NodeKind nodeKind = NodeKind::Standalone;
    std::string orchestratorHostname;
    uint16_t orchestratorPort = 8085;
    std::vector<std::byte> orchestratorPsk;
    std::string orchestratorRegionCode = "global";
    ServiceConnectionKind serviceConnectionKind = ServiceConnectionKind::DummyServiceConnection;
    uint16_t serviceConnectionMetadataReportIntervalMs = 4000;

    // Dummy Service Connection Backing Stores
    std::string dummyHmacKey = "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
    std::string dummyPreviewImagePath;

    // Glimesh Service Connection Backing Stores
    std::string glimeshServiceHostname = "localhost";
    uint16_t glimeshServicePort = 4000;
    bool glimeshServiceUseHttps = false;
    std::string glimeshServiceClientId;
    std::string glimeshServiceClientSecret;

    /* Private methods */
    /**
     * @brief Takes a hex string of format "010203FF" and converts it to an array of bytes.
     */
    std::vector<std::byte> hexStringToByteArray(std::string hexString);
};