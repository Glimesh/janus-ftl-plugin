/**
 * @file GlimeshServiceConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-15
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "GlimeshServiceConnection.h"

#include "../Utilities/FtlTypes.h"

#include <jansson.h>
#include <string.h>
#include <spdlog/spdlog.h>

#pragma region Constructor/Destructor
GlimeshServiceConnection::GlimeshServiceConnection(
    std::string hostname,
    uint16_t port,
    bool useHttps,
    std::string clientId,
    std::string clientSecret) : 
    hostname(hostname),
    port(port),
    useHttps(useHttps),
    clientId(clientId),
    clientSecret(clientSecret)
{ }
#pragma endregion

#pragma region ServiceConnection
void GlimeshServiceConnection::Init()
{
    std::stringstream baseUri;
    baseUri << (useHttps ? "https" : "http") << "://" << hostname << ":" << port;
    spdlog::info("Using Glimesh Service Connection @ {}", baseUri.str());

    // Try to auth
    getAccessToken();
}

Result<std::vector<std::byte>> GlimeshServiceConnection::GetHmacKey(uint32_t channelId)
{
    std::stringstream query;
    query << "query { channel(id: \"" << channelId << "\") { hmacKey } }";

    JsonPtr queryResult = runGraphQLQuery(query.str());
    json_t* jsonData = json_object_get(queryResult.get(), "data");
    if (jsonData != nullptr)
    {
        json_t* jsonChannel = json_object_get(jsonData, "channel");
        if (jsonChannel != nullptr)
        {
            json_t* jsonStreamKey = json_object_get(jsonChannel, "hmacKey");
            if (jsonStreamKey != nullptr && json_is_string(jsonStreamKey))
            {
                const char* keyValue = json_string_value(jsonStreamKey);
                return Result<std::vector<std::byte>>::Success(
                    std::vector<std::byte>(
                        reinterpret_cast<const std::byte*>(keyValue),
                        (reinterpret_cast<const std::byte*>(keyValue) + strlen(keyValue))));
            }
        }
    }

    return Result<std::vector<std::byte>>::Error(
        "Could not find a stream key for the given channel.");
}

Result<ftl_stream_id_t> GlimeshServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    std::stringstream query;
    query << "mutation { startStream(channelId: " << channelId << ") { id } }";

    JsonPtr queryResult = runGraphQLQuery(query.str());
    json_t* jsonData = json_object_get(queryResult.get(), "data");
    if (jsonData != nullptr)
    {
        json_t* jsonStream = json_object_get(jsonData, "startStream");
        if (jsonStream != nullptr)
        {
            json_t* jsonStreamId = json_object_get(jsonStream, "id");
            if (jsonStreamId != nullptr && json_is_string(jsonStreamId))
            {
                ftl_stream_id_t streamId = std::stoi(json_string_value(jsonStreamId));
                return Result<ftl_stream_id_t>::Success(streamId);
            }
        }
    }

    return Result<ftl_stream_id_t>::Error("Could not start stream.");
}

Result<void> GlimeshServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId,
    StreamMetadata metadata)
{
    // TODO: channelId -> streamId
    std::stringstream query;
    query << "mutation($streamId: ID!, $streamMetadata: StreamMetadataInput!) " << 
        "{ logStreamMetadata(streamId: $streamId, metadata: $streamMetadata) { id } }";

    // Create a json object to contain query variables
    JsonPtr queryVariables(json_pack(
        "{s:i, s:{s:s, s:s, s:i, s:i, s:i, s:i, s:i, s:i, s:i, s:s, s:s, s:s, s:i, s:i}}",
        "streamId",          streamId,
        "streamMetadata", 
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

    JsonPtr queryResult = runGraphQLQuery(query.str(), std::move(queryVariables));
    json_t* jsonData = json_object_get(queryResult.get(), "data");
    if (jsonData != nullptr)
    {
        json_t* jsonStream = json_object_get(jsonData, "logStreamMetadata");
        if (jsonStream != nullptr)
        {
            json_t* jsonStreamId = json_object_get(jsonStream, "id");
            if (jsonStreamId != nullptr)
            {
                // uint32_t updatedStreamId = json_integer_value(jsonStreamId);
                // TOTO: Handle error case
                return Result<void>::Success();
            }
        }
    }

    return Result<void>::Error("Error updating stream metadata.");
}

Result<void> GlimeshServiceConnection::EndStream(ftl_stream_id_t streamId)
{
    std::stringstream query;
    query << "mutation { endStream(streamId: " << streamId << ") { id } }";

    JsonPtr queryResult = runGraphQLQuery(query.str());
    json_t* jsonData = json_object_get(queryResult.get(), "data");
    if (jsonData != nullptr)
    {
        json_t* jsonStream = json_object_get(jsonData, "endStream");
        if (jsonStream != nullptr)
        {
            json_t* jsonStreamId = json_object_get(jsonStream, "id");
            if (jsonStreamId != nullptr)
            {
                // uint32_t endedStreamId = json_integer_value(jsonStreamId);
                // TODO: Handle error case
                return Result<void>::Success();
            }
        }
    }
    return Result<void>::Error("Error ending stream");
}

Result<void> GlimeshServiceConnection::SendJpegPreviewImage(
    ftl_stream_id_t streamId,
    std::vector<uint8_t> jpegData)
{
    std::stringstream query;
    query << "mutation { uploadStreamThumbnail(streamId: " << streamId << ", thumbnail: \"thumbdata\") { id } }";

    std::string fileContents(jpegData.begin(), jpegData.end());

    httplib::MultipartFormDataItems files
    {
        {
            .name = "thumbdata",
            .content = fileContents,
            .filename = "preview.jpg",
            .content_type = "image/jpeg"
        }
    };

    runGraphQLQuery(query.str(), nullptr, files);
    // TODO: Handle errors
    return Result<void>::Success();
}
#pragma endregion

#pragma region Private methods
httplib::Client GlimeshServiceConnection::getHttpClient()
{
    std::stringstream baseUri;
    baseUri << (useHttps ? "https" : "http") << "://" << hostname << ":" << port;
    return httplib::Client(baseUri.str().c_str());
}

std::string GlimeshServiceConnection::getAccessToken()
{
    std::lock_guard<std::mutex> lock(authMutex);

    // Do we already have an access token that hasn't expired?
    auto now = std::chrono::steady_clock::now();
    if (!accessToken.empty() && accessTokenExpirationTime - ACCESS_TOKEN_EXPIRATION_DELTA > now)
    {
        return accessToken;
    }

    // No? Let's fetch one.
    httplib::Params params
    {
        { "client_id", clientId },
        { "client_secret", clientSecret },
        { "grant_type", "client_credentials" },
        { "scope", "streamkey" }
    };
    auto requestId = httplib::detail::random_string(20);
    httplib::Headers headers = {
        { "x-request-id", requestId }
    };

    httplib::Client httpClient = getHttpClient();
    if (httplib::Result res = httpClient.Post("/api/oauth/token", headers, params))
    {
        if (res->status == 200)
        {
            // Try to parse out the access token
            json_error_t error;
            JsonPtr jsonBody(json_loads(res->body.c_str(), 0, &error));
            if (jsonBody.get() != nullptr)
            {
                // Extract access token
                json_t* accessTokenJson = json_object_get(jsonBody.get(), "access_token");
                accessToken = std::string(json_string_value(accessTokenJson));

                // Extract seconds until expiration
                json_t* expiresInJson = json_object_get(jsonBody.get(), "expires_in");
                json_int_t expiresInInt = json_integer_value(expiresInJson);
                auto expiresIn = std::chrono::seconds(json_integer_value(expiresInJson));

                // Extract creation time
                json_t* createdAtJson = json_object_get(jsonBody.get(), "created_at");
                std::string createdAtString = std::string(json_string_value(createdAtJson));

                // Calculate expiration time point using monotonic clock
                accessTokenExpirationTime = std::chrono::steady_clock::now() + expiresIn;

                spdlog::info("Received new access token: expires in {} seconds, created at {}, request id {}",
                    expiresInInt, createdAtString, requestId);
                return accessToken;
            }
        }
    }

    // TODO: Retry logic based on failure/status code
    spdlog::error("Failed to get new access token, request id: {}", requestId);
    throw std::runtime_error("Access token request failed!");
}

JsonPtr GlimeshServiceConnection::runGraphQLQuery(
    std::string query,
    JsonPtr variables,
    httplib::MultipartFormDataItems fileData)
{
    std::string queryString;

    // If we're doing a file upload, we pack this all into a multipart request
    if (fileData.size() > 0)
    {
        fileData.push_back(httplib::MultipartFormData {
            .name = "query",
            .content = query,
            .filename = "",
            .content_type = "application/json"
        });
        
    }
    // Otherwise, create a JSON blob for our GraphQL query to put into POST body
    else
    {
        
        JsonPtr queryJson(json_pack("{s:s, s:O?}", "query", query.c_str(), "variables", variables.get()));
        char* queryStr = json_dumps(queryJson.get(), 0);
        queryString = std::string(queryStr);
        free(queryStr);
    }

    // Make the request, and retry if necessary
    int numRetries = 0;
    while (true)
    {
        httplib::Client httpClient = getHttpClient();
        httpClient.set_bearer_token_auth(getAccessToken().c_str());

        JsonPtr result = nullptr;

        // If we're doing files, use a multipart http request
        if (fileData.size() > 0)
        {
            httplib::Result response = httpClient.Post("/api", fileData);
            result = processGraphQLResponse(response);
        }
        // otherwise, stick with post body
        else
        {
            httplib::Result response = httpClient.Post("/api", queryString, "application/json");
            result = processGraphQLResponse(response);
        }

        if (result != nullptr)
        {
            return result;
        }

        if (numRetries < MAX_RETRIES)
        {
            spdlog::warn("Attempt {} / {}: Glimesh file upload GraphQL query failed. "
                "Retrying in {} ms...", (numRetries + 1), MAX_RETRIES, TIME_BETWEEN_RETRIES.count());

            std::this_thread::sleep_for(TIME_BETWEEN_RETRIES);
            ++numRetries;
        }
        else
        {
            break;
        }
    }
    
    // We've exceeded our retry limit
    spdlog::error("Aborting Glimesh file upload GraphQL query after {} failed attempts.",
        MAX_RETRIES);

    throw ServiceConnectionCommunicationFailedException("Glimesh GraphQL query failed.");
}

JsonPtr GlimeshServiceConnection::processGraphQLResponse(httplib::Result result)
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
                errStr << "Could not parse GraphQL JSON response from Glimesh Service Connection: \n"
                    << result->body.c_str();
                throw ServiceConnectionCommunicationFailedException(errStr.str().c_str());
            }
            return jsonBody;
        }
        else
        {
            spdlog::warn("Glimesh service connection received status code {} when processing "
                "GraphQL query.", result->status);
            return nullptr;
        }
    }
    else
    {
        spdlog::warn("Glimesh service connection HTTP request failed.");
        return nullptr;
    }
}

#pragma endregion
