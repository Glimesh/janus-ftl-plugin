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

#include <chrono>
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
    RestServiceConnection = 2,
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
    std::chrono::milliseconds GetServiceConnectionMetadataReportInterval();
    uint32_t GetMaxAllowedBitsPerSecond(); 
    bool IsNackLostPacketsEnabled();

    // Dummy Service Connection Values
    std::vector<std::byte> GetDummyHmacKey();
    std::string GetDummyPreviewImagePath();

    // Glimesh Service Connection Values
    std::string GetGlimeshServiceHostname();
    uint16_t GetGlimeshServicePort();
    bool GetGlimeshServiceUseHttps();
    std::string GetGlimeshServiceClientId();
    std::string GetGlimeshServiceClientSecret();

    // REST Service Connection Values
    std::string GetRestServiceHostname();
    uint16_t GetRestServicePort();
    bool GetRestServiceUseHttps();
    std::string GetRestServicePathBase();
    std::string GetRestServiceAuthToken();

private:
    /* Backing stores */
    std::string myHostname;
    NodeKind nodeKind = NodeKind::Standalone;
    std::string orchestratorHostname;
    uint16_t orchestratorPort = 8085;
    std::vector<std::byte> orchestratorPsk;
    std::string orchestratorRegionCode = "global";
    ServiceConnectionKind serviceConnectionKind = ServiceConnectionKind::DummyServiceConnection;
    std::chrono::milliseconds serviceConnectionMetadataReportInterval = std::chrono::milliseconds(4000);
    uint32_t maxAllowedBitsPerSecond = 0;
    bool nackLostPackets = false;

    // Dummy Service Connection Backing Stores
    // "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456"
    std::vector<std::byte> dummyHmacKey = {
        std::byte('a'), std::byte('B'), std::byte('c'), std::byte('D'), std::byte('e'),
        std::byte('F'), std::byte('g'), std::byte('H'), std::byte('i'), std::byte('J'),
        std::byte('k'), std::byte('L'), std::byte('m'), std::byte('N'), std::byte('o'),
        std::byte('P'), std::byte('q'), std::byte('R'), std::byte('s'), std::byte('T'),
        std::byte('u'), std::byte('V'), std::byte('w'), std::byte('X'), std::byte('y'),
        std::byte('Z'), std::byte('1'), std::byte('2'), std::byte('3'), std::byte('4'),
        std::byte('5'), std::byte('6'),
    };
    std::string dummyPreviewImagePath;

    // Glimesh Service Connection Backing Stores
    std::string glimeshServiceHostname = "localhost";
    uint16_t glimeshServicePort = 4000;
    bool glimeshServiceUseHttps = false;
    std::string glimeshServiceClientId;
    std::string glimeshServiceClientSecret;

    // Rest Service Connection Backing Stores
    std::string restServiceHostname = "localhost";
    uint16_t restServicePort = 4000;
    bool restServiceUseHttps = false;
    std::string restServicePathBase = "/";
    std::string restServiceAuthToken;

    /* Private methods */
    /**
     * @brief Takes a hex string of format "010203FF" and converts it to an array of bytes.
     */
    std::vector<std::byte> hexStringToByteArray(std::string hexString);
};