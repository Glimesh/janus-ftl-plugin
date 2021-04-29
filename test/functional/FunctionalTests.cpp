/**
 * @file FunctionalTests.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-04-29
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include <memory>

#include "../../src/ConnectionCreators/UdpConnectionCreator.h"
#include "../../src/ConnectionListeners/TcpConnectionListener.h"
#include "../../src/JanusFtl.h"

/**
 * @brief Test fixture to handle common JanusFtl testing scenarios
 */
class FunctionalTestFixture
{
public:
    static FunctionalTestFixture* Instance;
    static constexpr uint16_t FTL_CONTROL_PORT = 8084;

    // Constructor
    FunctionalTestFixture()
    {
        FunctionalTestFixture::Instance = this;
        janusFtl = std::make_unique<JanusFtl>(
            const_cast<janus_plugin*>(&janus_ftl_plugin),
            std::move(std::make_unique<TcpConnectionListener>(FTL_CONTROL_PORT)),
            std::move(std::make_unique<UdpConnectionCreator>()),
            nullptr,
            "");
    }

    // Janus Plugin functions
    void JanusCreateSession(janus_plugin_session* handle, int* error)
    {
        janusFtl->CreateSession(handle, error);
    }

    janus_plugin_result* JanusHandleMessage(janus_plugin_session* handle, char* transaction,
        json_t* message, json_t* jsep)
    {
        return janusFtl->HandleMessage(handle, transaction, message, jsep);
    }

    json_t* JanusHandleAdminMessage(json_t* message)
    {
        return janusFtl->HandleAdminMessage(message);
    }

    void JanusSetupMedia(janus_plugin_session* handle)
    {
        janusFtl->SetupMedia(handle);
    }

    void JanusIncomingRtp(janus_plugin_session* handle, janus_plugin_rtp* packet)
    {
        janusFtl->IncomingRtp(handle, packet);
    }

    void JanusIncomingRtcp(janus_plugin_session* handle, janus_plugin_rtcp* packet)
    {
        janusFtl->IncomingRtcp(handle, packet);
    }

    void JanusDataReady(janus_plugin_session* handle)
    {
        janusFtl->DataReady(handle);
    }

    void JanusHangUpMedia(janus_plugin_session* handle)
    {
        janusFtl->HangUpMedia(handle);
    }

    void JanusDestroySession(janus_plugin_session* handle, int* error)
    {
        janusFtl->DestroySession(handle, error);
    }

    json_t* JanusQuerySession(janus_plugin_session* handle)
    {
        return janusFtl->QuerySession(handle);
    }

    static constexpr janus_plugin janus_ftl_plugin =
    {
        // Init/destroy
        .init = [](janus_callbacks* callback, const char* configPath) -> int { return 0; },
        .destroy = []() -> void {},

        // Metadata
        .get_api_compatibility = []() -> int { return JANUS_PLUGIN_API_VERSION; },
        .get_version = []() -> int { return 1; },
        .get_version_string = []() -> const char* { return "TEST"; },
        .get_description = []() -> const char* { return "TEST PLUGIN"; },
        .get_name = []() -> const char* { return "TEST PLUGIN"; },
        .get_author = []() -> const char* { return "TEST AUTHOR"; },
        .get_package = []() -> const char* { return "janus.ftl.test"; },

        // Plugin functionality
        .create_session = 
            [](janus_plugin_session* handle, int* error)
            {
                FunctionalTestFixture::Instance->JanusCreateSession(handle, error);
            },
        .handle_message = 
            [](janus_plugin_session* handle, char* transaction,
                json_t* message, json_t* jsep)
            {
                return FunctionalTestFixture::Instance->JanusHandleMessage(handle, transaction,
                    message, jsep);
            },
        .handle_admin_message = 
            [](json_t* message)
            {
                return FunctionalTestFixture::Instance->JanusHandleAdminMessage(message);
            },
        .setup_media = 
            [](janus_plugin_session* handle)
            {
                return FunctionalTestFixture::Instance->JanusSetupMedia(handle);
            },
        .incoming_rtp = 
            [](janus_plugin_session* handle, janus_plugin_rtp* packet)
            {
                return FunctionalTestFixture::Instance->JanusIncomingRtp(handle, packet);
            },
        .incoming_rtcp = 
            [](janus_plugin_session* handle, janus_plugin_rtcp* packet)
            {
                return FunctionalTestFixture::Instance->JanusIncomingRtcp(handle, packet);
            },
        .incoming_data = nullptr,
        .data_ready = 
            [](janus_plugin_session* handle)
            {
                return FunctionalTestFixture::Instance->JanusDataReady(handle);
            },
        .slow_link = nullptr,
        .hangup_media = 
            [](janus_plugin_session* handle)
            {
                return FunctionalTestFixture::Instance->JanusHangUpMedia(handle);
            },
        .destroy_session = 
            [](janus_plugin_session* handle, int* error)
            {
                return FunctionalTestFixture::Instance->JanusDestroySession(handle, error);
            },
        .query_session = 
            [](janus_plugin_session* handle)
            {
                return FunctionalTestFixture::Instance->JanusQuerySession(handle);
            },
    };
private:
    std::unique_ptr<JanusFtl> janusFtl;
};

FunctionalTestFixture* FunctionalTestFixture::Instance = nullptr;

TEST_CASE_METHOD(FunctionalTestFixture, "Does it work?")
{

}