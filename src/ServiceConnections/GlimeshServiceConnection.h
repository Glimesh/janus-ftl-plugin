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

#include "ServiceConnection.h"
#include "../Utilities/JanssonPtr.h"

#include <chrono>
#include <ctime>
#include <httplib.h>
#include <mutex>
#include <string>

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
    Result<std::vector<std::byte>> GetHmacKey(ftl_channel_id_t channelId) override;
    Result<ftl_stream_id_t> StartStream(ftl_channel_id_t channelId) override;
    Result<void> UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata) override;
    Result<void> EndStream(ftl_stream_id_t streamId) override;
    Result<void> SendJpegPreviewImage(ftl_stream_id_t streamId,
        std::vector<uint8_t> jpegData) override;

private:
    /* Private members */
    const int MAX_RETRIES = 5;
    const std::chrono::milliseconds TIME_BETWEEN_RETRIES = std::chrono::seconds(3);
    // Account for potential server-to-server clock skew and network delay by considering
    // an access token as expired well before its actual expiration time.
    const std::chrono::milliseconds ACCESS_TOKEN_EXPIRATION_DELTA = std::chrono::seconds(10);
    std::string hostname;
    uint16_t port;
    bool useHttps;
    std::string clientId;
    std::string clientSecret;
    std::string accessToken;
    std::chrono::steady_clock::time_point accessTokenExpirationTime;
    std::mutex authMutex;

    /* Private methods */
    httplib::Client getHttpClient();
    std::string getAccessToken();
    JsonPtr runGraphQLQuery(std::string query, JsonPtr variables = nullptr, httplib::MultipartFormDataItems fileData = httplib::MultipartFormDataItems());
    JsonPtr processGraphQLResponse(httplib::Result result);
};