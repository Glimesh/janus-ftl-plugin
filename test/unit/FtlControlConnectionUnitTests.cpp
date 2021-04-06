/**
 * @file FtlControlConnectionUnitTests.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include "../../src/FtlControlConnection.h"
#include "../mocks/MockConnectionTransport.h"
#include "../mocks/MockFtlControlConnectionManager.h"

/**
 * @brief Test fixture to expose some convenient helpers
 */
class FtlControlConnectionUnitTestsFixture
{
public:
    // Public types
    struct FtlControlConnectionHmacKeyRequest
    {
        ftl_channel_id_t ChannelId;
    };
    struct FtlControlConnectionMediaPortRequest
    {
        ftl_channel_id_t ChannelId;
        MediaMetadata MediaMetadataInfo;
        in_addr TargetAddr;
    };
    struct FtlControlConnectionState
    {
        bool HasStopped{ false };
        std::optional<FtlControlConnectionHmacKeyRequest> HmacKeyRequest;
        std::optional<FtlControlConnectionMediaPortRequest> MediaPortRequest;
    };

    // Constructor
    FtlControlConnectionUnitTestsFixture() :
        connectionManager(std::make_unique<MockFtlControlConnectionManager>(
            std::bind(
                &FtlControlConnectionUnitTestsFixture::handleControlConnectionStopped,
                this, std::placeholders::_1),
            std::bind(
                &FtlControlConnectionUnitTestsFixture::handleControlConnectionRequestedHmacKey,
                this, std::placeholders::_1, std::placeholders::_2),
            std::bind(
                &FtlControlConnectionUnitTestsFixture::handleControlConnectionRequestedMediaPort,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4)))
    { }

    // Public functions
    /**
     * @brief Creates a MockConnectionTransport and provides it as the transport for a new
     * FtlControlConnection instance.
     */
    std::pair<std::unique_ptr<FtlControlConnection>, MockConnectionTransport*>
        ConnectMockControlConnection()
    {
        auto mockTransport = std::make_unique<MockConnectionTransport>();
        MockConnectionTransport* mockTransportPtr = mockTransport.get();
        auto controlConnection = std::make_unique<FtlControlConnection>(connectionManager.get(),
            std::move(mockTransport));

        return { std::move(controlConnection), mockTransportPtr };
    }

    /**
     * @brief Retrieves the current state (if it exists) of the FtlControlConnection as reported
     * by the FtlControlConnectionManager
     */
    std::optional<FtlControlConnectionState> GetFtlControlConnectionState(
        FtlControlConnection* connection)
    {
        if (controlConnections.count(connection) <= 0)
        {
            return std::nullopt;
        }

        return controlConnections.at(connection);
    }

private:
    // Private fields
    std::unique_ptr<MockFtlControlConnectionManager> connectionManager;
    std::unordered_map<FtlControlConnection*, FtlControlConnectionState> controlConnections;

    // Private functions
    void handleControlConnectionStopped(FtlControlConnection* connection)
    {
        controlConnections[connection].HasStopped = true;
    }

    void handleControlConnectionRequestedHmacKey(FtlControlConnection* connection,
        ftl_channel_id_t channelId)
    {
        controlConnections[connection].HmacKeyRequest =
            FtlControlConnectionHmacKeyRequest {
                .ChannelId = channelId
            };
    }

    void handleControlConnectionRequestedMediaPort(FtlControlConnection* connection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr)
    {
        controlConnections[connection].MediaPortRequest =
            FtlControlConnectionMediaPortRequest {
                .ChannelId = channelId,
                .MediaMetadataInfo = mediaMetadata,
                .TargetAddr = targetAddr
            };
    }
};

TEST_CASE_METHOD(FtlControlConnectionUnitTestsFixture,
    "FtlControlConnection can negotiate valid control connections")
{
    auto [controlConnection, mockTransportPtr] = ConnectMockControlConnection();

    // Expect control connection to not yet have reported anything to the
    // FtlControlConnectionManager
    REQUIRE(GetFtlControlConnectionState(controlConnection.get()) == std::nullopt);

    // Start our FTL handshake
    std::vector<std::byte> hmacMessage = 
        {
            std::byte('H'),  std::byte('M'),  std::byte('A'),  std::byte('C'),
            std::byte('\r'), std::byte('\n'), std::byte('\r'), std::byte('\n'),
        };
    mockTransportPtr->InjectReceivedBytes(hmacMessage);
    // TODO: Mock transport needs to receive HMAC payload here
    // ...
}