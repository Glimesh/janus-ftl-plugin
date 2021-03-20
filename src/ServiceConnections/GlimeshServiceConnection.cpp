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

#pragma region Constructor/Destructor
GlimeshServiceConnection::GlimeshServiceConnection(
    std::string hostname,
    uint16_t port,
    bool useHttps,
    std::string clientId,
    std::string clientSecret) : 
    baseUri(fmt::format("{}://{}:{}", (useHttps ? "https" : "http"), hostname, port)),
    hostname(hostname),
    clientId(clientId),
    clientSecret(clientSecret)
{ }
#pragma endregion

#pragma region ServiceConnection
void GlimeshServiceConnection::Init()
{
    spdlog::info("Using Glimesh Service Connection @ {}", baseUri);

    // Try to auth
    ensureAuth(*getHttpClient());
}

Result<std::vector<std::byte>> GlimeshServiceConnection::GetHmacKey(uint32_t channelId)
{
    std::stringstream query;
    query << "query { channel(id: \"" << channelId << "\") { hmacKey } }";

    JsonPtr queryResult = runGraphQlQuery(query.str());
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

    JsonPtr queryResult = runGraphQlQuery(query.str());
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

Result<ServiceConnection::ServiceResponse> GlimeshServiceConnection::UpdateStreamMetadata(
    ftl_stream_id_t streamId, StreamMetadata metadata)
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

    JsonPtr queryResult = runGraphQlQuery(query.str(), std::move(queryVariables));

    // Check for GraphQL errors.
    json_t* errorsData = json_object_get(queryResult.get(), "errors");
    if (errorsData != nullptr)
    {
        // Try to extract the error message(s) so we can at least log it(them)
        if (!json_is_array(errorsData))
        {
            return Result<ServiceResponse>::Error(
                "Received GraphQL error of an unexpected format.");
        }
        size_t errorCount = json_array_size(errorsData);
        for (size_t i = 0; i < errorCount; ++i)
        {
            json_t* errorData = json_array_get(errorsData, i);
            json_t* errorMessageData = json_object_get(errorData, "message");
            if ((errorMessageData != nullptr) && json_is_string(errorMessageData))
            {
                spdlog::info("UpdateStreamMetadata received GraphQL error: {}",
                    json_string_value(errorMessageData));
            }
        }
        // Right now, we assume that an error means the stream has been shut down by the service.
        return Result<ServiceResponse>::Success(ServiceResponse::EndStream);
    }

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
                return Result<ServiceResponse>::Success(ServiceResponse::Ok);
            }
        }
    }

    return Result<ServiceResponse>::Error("Error updating stream metadata.");
}

Result<void> GlimeshServiceConnection::EndStream(ftl_stream_id_t streamId)
{
    std::stringstream query;
    query << "mutation { endStream(streamId: " << streamId << ") { id } }";

    JsonPtr queryResult = runGraphQlQuery(query.str());
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

    runGraphQlQuery(query.str(), nullptr, files);
    // TODO: Handle errors
    return Result<void>::Success();
}
#pragma endregion

#pragma region Private methods
std::unique_ptr<httplib::Client> GlimeshServiceConnection::getHttpClient() {
    auto client = std::make_unique<httplib::Client>(baseUri.c_str());
    client->set_socket_options([](socket_t sock) {
        // TODO: Remove once yhirose/cpp-httplib#873 is resolved
        struct timeval tv{};
        tv.tv_sec = DEFAULT_SOCKET_RECEIVE_TIMEOUT_SEC;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Set default options from cpp-httplib
        httplib::default_socket_options(sock);
    });
    return client;
}

void GlimeshServiceConnection::ensureAuth(httplib::Client& httpClient)
{
    std::lock_guard<std::mutex> lock(authMutex);

    // Do we already have an access token that hasn't expired?
    if (accessToken.length() > 0)
    {
        std::time_t currentTime = std::time(nullptr);
        if (currentTime < accessTokenExpirationTime)
        {
            httpClient.set_bearer_token_auth(accessToken.c_str());
            return;
        }
    }
    
    // No? Let's fetch one.
    httplib::Params params
    {
        { "client_id", clientId },
        { "client_secret", clientSecret },
        { "grant_type", "client_credentials" },
        { "scope", "streamkey" }
    };

    if (httplib::Result res = httpClient.Post("/api/oauth/token", params))
    {
        if (res->status == 200)
        {
            // Try to parse out the access token
            // TODO: Parse out expiration time
            json_error_t error;
            JsonPtr jsonBody(json_loads(res->body.c_str(), 0, &error));
            if (jsonBody.get() != nullptr)
            {
                // Extract access token
                json_t* accessTokenJson = json_object_get(jsonBody.get(), "access_token");
                accessToken = std::string(json_string_value(accessTokenJson));

                // Extract time to expiration
                json_t* expiresInJson = json_object_get(jsonBody.get(), "expires_in");
                uint32_t expiresIn = json_integer_value(expiresInJson);

                // Extract creation time
                json_t* createdAtJson = json_object_get(jsonBody.get(), "created_at");
                std::string createdAtStr = std::string(json_string_value(createdAtJson));
                tm createdAtTime = parseIso8601DateTime(createdAtStr);

                // Calculate expiration time
                std::time_t expirationTime = std::mktime(&createdAtTime);
                expirationTime += expiresIn;
                accessTokenExpirationTime = expirationTime;

                std::time_t currentTime = std::time(nullptr);
                spdlog::info("Received new access token, expires in {} - {} = {} seconds",
                    expirationTime, currentTime, (expirationTime - currentTime));

                httpClient.set_bearer_token_auth(accessToken.c_str());
                return;
            }
        }
    }

    // TODO: Retry logic based on failure/status code
    throw std::runtime_error("Access token request failed!");
}

JsonPtr GlimeshServiceConnection::runGraphQlQuery(
    std::string query,
    JsonPtr variables,
    httplib::MultipartFormDataItems fileData)
{
    std::unique_ptr<httplib::Client> httpClient = getHttpClient();

    // Make sure we have a valid access token
    ensureAuth(*httpClient);

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
        JsonPtr result = nullptr;

        // If we're doing files, use a multipart http request
        if (fileData.size() > 0)
        {
            httplib::Result response = httpClient->Post("/api", fileData);
            result = processGraphQlResponse(response);
        }
        // otherwise, stick with post body
        else
        {
            httplib::Result response = httpClient->Post("/api", queryString, "application/json");
            result = processGraphQlResponse(response);
        }

        if (result != nullptr)
        {
            return result;
        }

        if (numRetries < MAX_RETRIES)
        {
            spdlog::warn("Attempt {} / {}: Glimesh GraphQL query failed. "
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
    spdlog::error("Aborting Glimesh GraphQL query after {} failed attempts.",
        MAX_RETRIES);

    throw ServiceConnectionCommunicationFailedException("Glimesh GraphQL query failed.");
}

JsonPtr GlimeshServiceConnection::processGraphQlResponse(const httplib::Result& result)
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
        spdlog::warn("Glimesh service connection HTTP request failed with error {}",
            result.error());
        return nullptr;
    }
}

// Thanks StackOverflow friends
// https://stackoverflow.com/a/26896792/2874534
tm GlimeshServiceConnection::parseIso8601DateTime(std::string dateTimeString)
{
    int y, M, d, h, m, tzh, tzm;
    float s;
    if (6 < sscanf(dateTimeString.c_str(), "%d-%d-%dT%d:%d:%f%d:%dZ", &y, &M, &d, &h, &m, &s, &tzh, &tzm))
    {
        if (tzh < 0)
        {
            tzm = -tzm; // Fix the sign on minutes.
        }
    }

    tm time { 0 };
    time.tm_year = y - 1900; // Year since 1900
    time.tm_mon = M - 1;     // 0-11
    time.tm_mday = d;        // 1-31
    time.tm_hour = h;        // 0-23
    time.tm_min = m;         // 0-59
    time.tm_sec = (int)s;    // 0-60

    return time;
}
#pragma endregion
