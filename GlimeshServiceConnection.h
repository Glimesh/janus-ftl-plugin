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
#include <mutex>
#include "ServiceConnection.h"
#include "JanssonPtr.h"

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
    std::string GetHmacKey(ftl_channel_id_t channelId) override;
    ftl_stream_id_t StartStream(ftl_channel_id_t channelId) override;
    void UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata) override;
    void EndStream(ftl_stream_id_t streamId) override;
    void SendJpegPreviewImage(ftl_stream_id_t streamId, std::vector<uint8_t> jpegData) override;

private:
    /* Private members */
    const int MAX_RETRIES = 5;
    const int TIME_BETWEEN_RETRIES_MS = 3000;
    std::string hostname;
    uint16_t port;
    bool useHttps;
    std::string clientId;
    std::string clientSecret;
    std::string accessToken;
    std::time_t accessTokenExpirationTime;
    std::mutex authMutex;

    /* Private methods */
    httplib::Client getHttpClient();
    void ensureAuth();
    JsonPtr runGraphQlQuery(std::string query, JsonPtr variables = nullptr, httplib::MultipartFormDataItems fileData = httplib::MultipartFormDataItems());
    JsonPtr processGraphQlResponse(httplib::Result result);
    tm parseIso8601DateTime(std::string dateTimeString);
};