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
    const int MAX_RETRIES = 10;
    const int TIME_BETWEEN_RETRIES_MS = 3000;
    httplib::Client httpClient;
    std::string hostname;
    uint16_t port;
    bool useHttps;
    std::string clientId;
    std::string clientSecret;
    std::string accessToken;
    std::time_t accessTokenExpirationTime;
    std::mutex authMutex;

    /* Private methods */
    void ensureAuth();
    JsonPtr runGraphQlQuery(std::string query, JsonPtr variables = nullptr, httplib::MultipartFormDataItems fileData = httplib::MultipartFormDataItems());
    JsonPtr processGraphQlResponse(const httplib::Result& result);
    tm parseIso8601DateTime(std::string dateTimeString);
};