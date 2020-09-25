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
#include "JanssonPtr.h"
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
    httpClient = std::make_unique<httplib::Client>(baseUri.str().c_str());
    JANUS_LOG(LOG_INFO, "FTL: Using Glimesh Service Connection @ %s\n", baseUri.str().c_str());

    // Try to auth
    ensureAuth();
}

std::string GlimeshServiceConnection::GetHmacKey(uint32_t userId)
{
    ensureAuth();
    return "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
}

uint32_t GlimeshServiceConnection::CreateStream(uint32_t userId)
{
    ensureAuth();
    return 1;
}

void GlimeshServiceConnection::StartStream(uint32_t streamId)
{
    ensureAuth();
}

void GlimeshServiceConnection::UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata)
{
    ensureAuth();
}

void GlimeshServiceConnection::EndStream(uint32_t streamId)
{
    ensureAuth();
}
#pragma endregion

#pragma region Private methods
void GlimeshServiceConnection::ensureAuth()
{
    // Do we already have an access token that hasn't expired?
    // TODO: Check expiration
    if (accessToken.length() > 0)
    {
        return;
    }
    
    // No? Let's fetch one.
    httplib::Params params
    {
        { "client_id", clientId },
        { "client_secret", clientSecret },
        { "grant_type", "client_credentials" },
        { "scope", "streamkey" }
    };

    if (httplib::Result res = httpClient->Post("/api/oauth/token", params))
    {
        if (res->status == 200)
        {
            // Try to parse out the access token
            // TODO: Parse out expiration time
            json_error_t error;
            JsonPtr jsonBody(json_loads(res->body.c_str(), 0, &error));
            if (jsonBody.get() != nullptr)
            {
                json_t* accessTokenJson = json_object_get(jsonBody.get(), "access_token");
                accessToken = std::string(json_string_value(accessTokenJson));
                JANUS_LOG(LOG_INFO, "FTL: Received access token: %s\n", accessToken.c_str());
                return;
            }
        }
    }
    else
    {
        httplib::Error err = res.error();
        throw std::runtime_error("HTTP request failed!");
    }

    throw std::runtime_error("Access token request failed!");
}
#pragma endregion