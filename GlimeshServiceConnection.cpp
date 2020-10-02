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
#include <jansson.h>
extern "C"
{
    #include <debug.h>
}

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
    JANUS_LOG(LOG_INFO, "FTL: Using Glimesh Service Connection @ %s\n", baseUri.str().c_str());

    // Try to auth
    ensureAuth();
}

std::string GlimeshServiceConnection::GetHmacKey(uint32_t channelId)
{
    std::stringstream query;
    query << "query { channel(id: \"" << channelId << "\") { streamKey } }";

    JsonPtr queryResult = runGraphQlQuery(query.str());
    json_t* jsonData = json_object_get(queryResult.get(), "data");
    if (jsonData != nullptr)
    {
        json_t* jsonChannel = json_object_get(jsonData, "channel");
        if (jsonChannel != nullptr)
        {
            json_t* jsonStreamKey = json_object_get(jsonChannel, "streamKey");
            if (jsonStreamKey != nullptr)
            {
                return std::string(json_string_value(jsonStreamKey));
            }
        }
    }

    return std::string(); // Empty string means we couldn't find one
}

ftl_stream_id_t GlimeshServiceConnection::StartStream(ftl_channel_id_t channelId)
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
            if (jsonStreamId != nullptr)
            {
                ftl_stream_id_t streamId = std::stoi(json_string_value(jsonStreamId));
                return streamId;
            }
        }
    }

    return 0;
}

void GlimeshServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata)
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
            }
        }
    }
}

void GlimeshServiceConnection::EndStream(ftl_stream_id_t streamId)
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
            }
        }
    }
}
#pragma endregion

#pragma region Private methods
httplib::Client GlimeshServiceConnection::getHttpClient()
{
    std::stringstream baseUri;
    baseUri << (useHttps ? "https" : "http") << "://" << hostname << ":" << port;
    httplib::Client client = httplib::Client(baseUri.str().c_str());

    if (accessToken.length() > 0)
    {
        httplib::Headers headers
        {
            {"Authorization", "Bearer " + accessToken}
        };
        client.set_default_headers(headers);
    }

    return client;
}

void GlimeshServiceConnection::ensureAuth()
{
    std::lock_guard<std::mutex> lock(authMutex);

    // Do we already have an access token that hasn't expired?
    // TODO: Check expiration
    if (accessToken.length() > 0)
    {
        std::time_t currentTime = std::time(nullptr);
        if (currentTime < accessTokenExpirationTime)
        {
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

    httplib::Client httpClient = getHttpClient();
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
                JANUS_LOG(LOG_INFO, "FTL: Received access token: %s\n", accessToken.c_str());

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
                return;
            }
        }
    }

    // TODO: Retry logic based on failure/status code
    throw std::runtime_error("Access token request failed!");
}

JsonPtr GlimeshServiceConnection::runGraphQlQuery(std::string query, JsonPtr variables)
{
    // Make sure we have a valid access token
    ensureAuth();

    // Create a JSON blob for our GraphQL query
    JsonPtr queryJson(json_pack("{s:s, s:o?}", "query", query.c_str(), "variables", variables.get()));
    char* queryStr = json_dumps(queryJson.get(), 0);
    std::string queryString(queryStr);
    free(queryStr);

    // Make the request
    httplib::Client httpClient = getHttpClient();
    if (httplib::Result res = httpClient.Post("/api", queryString, "application/json"))
    {
        if (res->status == 200)
        {
            // Try to parse out the response
            json_error_t error;
            JsonPtr jsonBody(json_loads(res->body.c_str(), 0, &error));
            if (jsonBody.get() == nullptr)
            {
                throw std::runtime_error("Could not parse GraphQL JSON response.");
            }
            return jsonBody;
        }
        else
        {
            // TODO: Retry/re-auth logic based on status code
            throw std::runtime_error("GraphQL HTTP request received unexpected status code!");
        }
    }
    else
    {
        throw std::runtime_error("GraphQL HTTP request failed!");
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

    tm time;
    time.tm_year = y - 1900; // Year since 1900
    time.tm_mon = M - 1;     // 0-11
    time.tm_mday = d;        // 1-31
    time.tm_hour = h;        // 0-23
    time.tm_min = m;         // 0-59
    time.tm_sec = (int)s;    // 0-60

    return time;
}
#pragma endregion