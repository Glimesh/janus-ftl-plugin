/**
 * @file GlimeshServiceConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-15
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <string>
#include <chrono>
#include <ctime>
#include <httplib.h>
#include "ServiceConnection.h"

/**
 * @brief
 *  GlimeshServiceConnection is a service connection implementation for the Glimesh.tv
 *  platform.
 */
class GlimeshServiceConnection : 
    public ServiceConnection
{
public:
    /* Constructor/Destructor */
    GlimeshServiceConnection(
        std::string hostname,
        uint16_t port,
        bool useHttps,
        std::string clientId,
        std::string clientSecret);

    /* ServiceConnection */
    void Init() override;
    std::string GetHmacKey(uint32_t userId) override;
    uint32_t CreateStream(uint32_t userId) override;
    void StartStream(uint32_t streamId) override;
    void UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata) override;
    void EndStream(uint32_t streamId) override;

private:
    /* Private members */
    std::string hostname;
    uint16_t port;
    bool useHttps;
    std::string clientId;
    std::string clientSecret;
    std::unique_ptr<httplib::Client> httpClient;
    std::string accessToken;
    std::time_t accessTokenExpirationTime;

    /* Private methods */
    void ensureAuth();
};