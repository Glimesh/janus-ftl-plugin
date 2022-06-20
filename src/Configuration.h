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
#include <optional>
#include <string>
#include <vector>

enum class NodeKind
{
    Standalone = 0,
    Ingest = 1,
    // Relay = 2,
    Edge = 3,
    Combo = 4,
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
    /**
     * @brief Range of expected delay between server capturing a frame and clients receiving it.
     * 
     * Note min/max delay are in units of 10ms as specified by the playout-delay specification, but
     * the constructor takes the chrono::duration type for clarity and convenience.
     * 
     * Sent to clients via an experimental RTP extension only implemented for Chrome. Can be used to
     * suggest a bounded range client should delay before rendering a frame. In theory the client
     * should determine an appropriate delay to account for network jitter and rendering time.
     * 
     * However, we have seen Chrome be wrong when choosing a delay, and there are other use cases
     * where bounding the minimum or maximum delay can be useful. See the RFC for more details.
     * https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/playout-delay
     * 
     * Reasonable values range from 0 to 10,000 milliseconds (rounded to a granularity of 10ms). The
     * ideal value depends on expected network delay and jitter clients will experience. Generally
     * a minimum of 100ms-400ms and a maximum of a few seconds is a good starting range. 
     */
    struct PlayoutDelay
    {
public:
        PlayoutDelay(std::chrono::milliseconds min_ms, std::chrono::milliseconds max_ms);

        /* Public methods */
        uint16_t MinDelay() {
            return min;
        }
        uint16_t MaxDelay() {
            return max;
        }

private:
        uint16_t min;
        uint16_t max;
    };

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
    uint32_t GetRollingSizeAvgMs();
    bool IsNackLostPacketsEnabled();
    std::optional<PlayoutDelay> GetPlayoutDelay();

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
    /* Constants */
    // Playout delay configuration can only used if your Janus version supports the playout-delay
    // RTP extension, hence the compiler flag. We use this constant to print a warning to the user
    // if they set a delay configuration but it is not being used.
    #if defined(JANUS_PLAYOUT_DELAY_SUPPORT)
    static constexpr bool PLAYOUT_DELAY_SUPPORT = true;
    #else
    static constexpr bool PLAYOUT_DELAY_SUPPORT = false;
    #endif

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
    uint32_t rollingSizeAvgMs = 2000;
    bool nackLostPackets = false;
    std::optional<PlayoutDelay> playoutDelay;

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

/**
 * @brief Exception describing invalid configuration values
 */
struct InvalidConfigurationException : std::runtime_error
{
    InvalidConfigurationException(const char *message) throw() : std::runtime_error(message)
    {
    }
};
