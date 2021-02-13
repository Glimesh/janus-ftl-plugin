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
    std::stringstream url;
    url << "/start/" << channelId;

    JsonPtr result = runRestPostRequest(url.str());
    json_t* jsonStreamId = json_object_get(result.get(), "streamId");

    // TODO: Allow strings and ints from JSON
    if (jsonStreamId != nullptr && json_is_string(jsonStreamId))
    {
        ftl_stream_id_t streamId = std::stoi(json_string_value(jsonStreamId));
        return Result<ftl_stream_id_t>::Success(streamId);
    }

    return Result<ftl_stream_id_t>::Error("Could not start stream.");
}

Result<void> RestServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId,
    StreamMetadata metadata)
{
    JsonPtr streamMetadata(json_pack(
        "{s:s, s:s, s:i, s:i, s:i, s:i, s:i, s:i, s:i, s:s, s:s, s:s, s:i, s:i}",
        "audioCodec",        metadata.audioCodec.c_str(),
        "ingestServer",      metadata.ingestServerHostname.c_str(),
        "ingestViewers",     metadata.numActiveViewers,
        "lostPackets",       metadata.numPacketsLost,
        "nackPackets",       metadata.numPacketsNacked,
        "recvPackets",       metadata.numPacketsReceived,
        "sourceBitrate",     metadata.currentSourceBitrateBps,
        "sourcePing",        metadata.streamerToIngestPingMs,
        "streamTimeSeconds", metadata.streamTimeSeconds,
        "vendorName",        metadata.streamerClientVendorName.c_str(),
        "vendorVersion",     metadata.streamerClientVendorVersion.c_str(),
        "videoCodec",        metadata.videoCodec.c_str(),
        "videoHeight",       metadata.videoHeight,
        "videoWidth",        metadata.videoWidth
    ));

    std::stringstream url;
    url << "/metadata/" << streamId;

    runRestPostRequest(url.str(), std::move(streamMetadata));
    return Result<void>::Success();
}

Result<void> RestServiceConnection::EndStream(ftl_stream_id_t streamId)
{
    std::stringstream url;
    url << "/end/" << streamId;
    runRestPostRequest(url.str());

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

#pragma region Private methods
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
    std::string bodyString;
    if (body)
    {
        char* bodyStr = json_dumps(body.get(), 0);
        bodyString = std::string(bodyStr);
        free(bodyStr);
    }
    else
    {
        bodyString = "{}";
    }

    // If we're doing a file upload, we pack this all into a multipart request
    if (fileData.size() > 0)
    {
        fileData.push_back(httplib::MultipartFormData {
            .name = "body",
            .content = bodyString,
            .filename = "",
            .content_type = "application/json"
        });
    }

    // Make the request, and retry if necessary
    int numRetries = 0;
    while (true)
    {
        httplib::Client httpClient = getHttpClient();
        JsonPtr result = nullptr;

        // If we're doing files, use a multipart http request
        if (fileData.size() > 0)
        {
            httplib::Result response = httpClient.Post(url.c_str(), fileData);
            result = processRestResponse(response);
        }
        // otherwise, stick with post body
        else
        {
            httplib::Result response = httpClient.Post(url.c_str(), bodyString, "application/json");
            result = processRestResponse(response);
        }

        if (result != nullptr)
        {
            return result;
        }

        if (numRetries < MAX_RETRIES)
        {
            spdlog::warn("Attempt {} / {}: REST POST request failed. "
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
    spdlog::error("Aborting REST POST request after {} failed attempts.",
        MAX_RETRIES);

    throw ServiceConnectionCommunicationFailedException("REST POST request failed.");
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
