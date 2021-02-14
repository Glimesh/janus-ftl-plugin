/**
 * @file RestServiceConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 *
 * @copyright Copyright (c) 2020 Hayden McAfee
 *
 */

#pragma once

#include "ServiceConnection.h"
#include "../Utilities/JanssonPtr.h"

#include <httplib.h>
#include <string>

/**
 * @brief
 * RestServiceConnection is a service connection implementation for a generic REST
 * API server.
 */
class RestServiceConnection : public ServiceConnection
{
public:
    /* Constructor/Destructor */
    RestServiceConnection(
        std::string hostname,
        uint16_t port,
        bool useHttps,
        std::string pathBase,
        std::string authToken);

    // ServiceConnection
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
    const int TIME_BETWEEN_RETRIES_MS = 3000;
    std::string hostname;
    uint16_t port;
    bool useHttps;
    std::string pathBase;
    std::string authToken;

    /* Private methods */
    std::string resolvePathBase();
    std::string createBaseUri(bool includeBase);
    std::string constructPath(std::string path);

    httplib::Client getHttpClient();
    httplib::Result runGetRequest(std::string url);
    httplib::Result runPostRequest(std::string url, JsonPtr body = nullptr, httplib::MultipartFormDataItems fileData = httplib::MultipartFormDataItems());
    JsonPtr decodeRestResponse(httplib::Result result);
};
