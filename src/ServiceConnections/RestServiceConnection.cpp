/**
 * @file RestServiceConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 *
 * @copyright Copyright (c) 2020 Hayden McAfee
 *
 */

#include "RestServiceConnection.h"

#include "../Utilities/FtlTypes.h"

#include <fstream>
#include <iostream>
#include <jansson.h>
#include <linux/limits.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

#pragma region Constructor/Destructor
RestServiceConnection::RestServiceConnection(
    std::string hostname,
    uint16_t port,
    bool useHttps,
    std::string authToken)
:
    hostname(hostname),
    port(port),
    useHttps(useHttps),
    authToken(authToken)
{ }
#pragma endregion

#pragma region Public methods
void RestServiceConnection::Init()
{
    std::stringstream baseUri;
    baseUri << (useHttps ? "https" : "http") << "://" << hostname << ":" << port;
    spdlog::info("Using REST Service Connection @ {}", baseUri.str());
}

Result<std::vector<std::byte>> RestServiceConnection::GetHmacKey(ftl_channel_id_t channelId)
{
    std::stringstream url;
    url << "/hmac/" << channelId;

    JsonPtr result = runRestGetRequest(url.str());
    json_t* jsonStreamKey = json_object_get(result.get(), "hmacKey");

    if (jsonStreamKey != nullptr && json_is_string(jsonStreamKey))
    {
        const char* keyValue = json_string_value(jsonStreamKey);
        return Result<std::vector<std::byte>>::Success(
            std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(keyValue),
                (reinterpret_cast<const std::byte*>(keyValue) + strlen(keyValue))));
    }

    return Result<std::vector<std::byte>>::Error(
        "Could not find a stream key for the given channel.");
}

Result<ftl_stream_id_t> RestServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    // TODO: Un-hardcode stream ID
    return Result<ftl_stream_id_t>::Success(0);
}

Result<void> RestServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId,
    StreamMetadata metadata)
{
    // TODO
    return Result<void>::Success();
}

Result<void> RestServiceConnection::EndStream(ftl_stream_id_t streamId)
{
    // TODO
    return Result<void>::Success();
}

Result<void> RestServiceConnection::SendJpegPreviewImage(
    ftl_stream_id_t streamId,
    std::vector<uint8_t> jpegData)
{
    // TODO
    return Result<void>::Success();
}
#pragma endregion

#pragma Private methods
httplib::Client RestServiceConnection::getHttpClient()
{
    std::stringstream baseUri;
    baseUri << (useHttps ? "https" : "http") << "://" << hostname << ":" << port;
    httplib::Client client = httplib::Client(baseUri.str().c_str());

    if (authToken.length() > 0)
    {
        httplib::Headers headers
        {
            {"Authorization", authToken}
        };
        client.set_default_headers(headers);
    }

    return client;
}

JsonPtr RestServiceConnection::runRestGetRequest(std::string url)
{
    // Make the request, and retry if necessary
    int numRetries = 0;
    while (true)
    {
        httplib::Client httpClient = getHttpClient();
        JsonPtr result = nullptr;

        httplib::Result response = httpClient.Get(url.c_str());
        result = processRestResponse(response);

        if (result != nullptr)
        {
            return result;
        }

        if (numRetries < MAX_RETRIES)
        {
            spdlog::warn("Attempt {} / {}: REST GET request failed. "
                "Retrying in {} ms...", (numRetries + 1), MAX_RETRIES, TIME_BETWEEN_RETRIES_MS);

            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_BETWEEN_RETRIES_MS));
            ++numRetries;
        }
        else
        {
            break;
        }
    }

    // We've exceeded our retry limit
    spdlog::error("Aborting REST GET request after {} failed attempts.",
        MAX_RETRIES);

    throw ServiceConnectionCommunicationFailedException("REST GET request failed.");
}

JsonPtr RestServiceConnection::runRestPostRequest(
    std::string url,
    JsonPtr body,
    httplib::MultipartFormDataItems fileData)
{
    // TODO
    return nullptr;
}

JsonPtr RestServiceConnection::processRestResponse(httplib::Result result)
{
    if (result)
    {
        if (result->status == 200)
        {
            // Try to parse out the response
            json_error_t error;
            JsonPtr jsonBody(json_loads(result->body.c_str(), 0, &error));
            if (jsonBody.get() == nullptr)
            {
                // If we can't parse the JSON of a successful request, retrying won't help,
                // so we throw here.
                std::stringstream errStr;
                errStr << "Could not parse JSON response from REST Service Connection: \n"
                    << result->body.c_str();
                throw ServiceConnectionCommunicationFailedException(errStr.str().c_str());
            }
            return jsonBody;
        }
        else
        {
            spdlog::warn("REST service connection received status code {} when processing "
                "request.", result->status);
            return nullptr;
        }
    }
    else
    {
        spdlog::warn("REST service connection HTTP request failed.");
        return nullptr;
    }
}
#pragma endregion
