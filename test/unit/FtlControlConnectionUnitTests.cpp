/**
 * @file FtlControlConnectionUnitTests.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include <memory>
#include <openssl/hmac.h>
#include <optional>
#include <unordered_map>
#include <utility>

#include "../../src/FtlControlConnection.h"
#include "../../src/Utilities/Util.h"
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

protected:
    // Protected fields
    static constexpr int FTL_PROTOCOL_VERSION_MAJOR = 0;
    static constexpr int FTL_PROTOCOL_VERSION_MINOR = 9;

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
            FtlControlConnectionHmacKeyRequest
            {
                .ChannelId = channelId
            };
    }

    void handleControlConnectionRequestedMediaPort(FtlControlConnection* connection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr)
    {
        controlConnections[connection].MediaPortRequest =
            FtlControlConnectionMediaPortRequest
            {
                .ChannelId = channelId,
                .MediaMetadataInfo = mediaMetadata,
                .TargetAddr = targetAddr
            };
    }
};

TEST_CASE_METHOD(FtlControlConnectionUnitTestsFixture,
    "FtlControlConnection can negotiate valid control connections")
{
    uint16_t mediaPort = 9876;
    ftl_channel_id_t channelId = 1234;
    MediaMetadata metadata
    {
        .VendorName = "TEST",
        .VendorVersion = "0.0.0",
        .HasVideo = true,
        .HasAudio = true,
        .VideoCodec = VideoCodecKind::H264,
        .AudioCodec = AudioCodecKind::Opus,
        .VideoWidth = 1920,
        .VideoHeight = 1080,
        .VideoSsrc = 1235,
        .AudioSsrc = 1234,
        .VideoPayloadType = 96,
        .AudioPayloadType = 97,
    };

    // Come up with a stream HMAC key
    std::vector<std::byte> hmacKey = 
    {
        std::byte(0x00), std::byte(0x01), std::byte(0x02), std::byte(0x03),
        std::byte(0x04), std::byte(0x05), std::byte(0x06), std::byte(0x07),
        std::byte(0x08), std::byte(0x09), std::byte(0x0A), std::byte(0x0B),
        std::byte(0x0C), std::byte(0x0D), std::byte(0x0E), std::byte(0x0F),
    };

    // Connect!
    auto [controlConnection, mockTransportPtr] = ConnectMockControlConnection();

    // Expect control connection to not yet have reported anything to the
    // FtlControlConnectionManager
    REQUIRE(GetFtlControlConnectionState(controlConnection.get()) == std::nullopt);

    // Record every payload that was written to the mock transport connection
    std::span<std::byte> lastPayloadReceived;
    mockTransportPtr->SetOnWrite(
        [&lastPayloadReceived](const std::span<std::byte>& bytes)
        {
            lastPayloadReceived = bytes;
            return Result<void>::Success();
        });

    // Start our FTL handshake
    mockTransportPtr->InjectReceivedBytes("HMAC\r\n\r\n");
    // We receive a response payload immediately on the same thread via
    // MockConnectionTransport::Write
    REQUIRE(lastPayloadReceived.size() > 4);
    std::string responseCode(reinterpret_cast<char*>(lastPayloadReceived.data()),
        (reinterpret_cast<char*>(lastPayloadReceived.data()) + 3));
    REQUIRE(responseCode == "200");
    std::string hmacPayloadString(reinterpret_cast<char*>((lastPayloadReceived.data()) + 4),
        (reinterpret_cast<char*>(lastPayloadReceived.data()) + lastPayloadReceived.size() - 1));
    std::vector<std::byte> hmacPayloadBytes = Util::HexStringToByteArray(hmacPayloadString);

    // Generate our HMAC response
    std::byte hmacBuffer[512];
    uint32_t hmacBufferLength;
    HMAC(
        EVP_sha512(),
        hmacKey.data(),
        hmacKey.size(),
        reinterpret_cast<const unsigned char*>(hmacPayloadBytes.data()),
        hmacPayloadBytes.size(),
        reinterpret_cast<unsigned char*>(hmacBuffer),
        &hmacBufferLength);
    std::string hmacBufferString = Util::ByteArrayToHexString(&hmacBuffer[0], hmacBufferLength);
    std::string connectMessage = fmt::format("CONNECT {} ${}\r\n\r\n", channelId, hmacBufferString);
    mockTransportPtr->InjectReceivedBytes(connectMessage);

    // Verify that the control connection has requested an HMAC key for this channel
    std::optional<FtlControlConnectionState> controlState = 
        GetFtlControlConnectionState(controlConnection.get());
    REQUIRE(controlState.has_value());
    REQUIRE(controlState.value().HmacKeyRequest.has_value());
    REQUIRE(controlState.value().HmacKeyRequest.value().ChannelId == channelId);

    // Provide the HMAC key to the control connection
    controlConnection->ProvideHmacKey(hmacKey);

    // Verify we received a response
    REQUIRE(lastPayloadReceived.size() == 4);
    responseCode = std::string(reinterpret_cast<char*>(lastPayloadReceived.data()),
        (reinterpret_cast<char*>(lastPayloadReceived.data()) + 3));
    REQUIRE(responseCode == "200");

    // Send metadata
    mockTransportPtr->InjectReceivedBytes(fmt::format(
        "ProtocolVersion: {}.{}\r\n\r\n",
        FTL_PROTOCOL_VERSION_MAJOR,
        FTL_PROTOCOL_VERSION_MINOR));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VendorName: {}\r\n\r\n", metadata.VendorName));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VendorVersion: {}\r\n\r\n", metadata.VendorVersion));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("Video: {}\r\n\r\n", metadata.HasVideo ? "true" : "false"));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VideoCodec: {}\r\n\r\n",
            SupportedVideoCodecs::VideoCodecString(metadata.VideoCodec)));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VideoHeight: {}\r\n\r\n", metadata.VideoHeight));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VideoWidth: {}\r\n\r\n", metadata.VideoWidth));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VideoPayloadType: {}\r\n\r\n", metadata.VideoPayloadType));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("VideoIngestSSRC: {}\r\n\r\n", metadata.VideoSsrc));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("Audio: {}\r\n\r\n", metadata.HasAudio ? "true" : "false"));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("AudioCodec: {}\r\n\r\n",
            SupportedAudioCodecs::AudioCodecString(metadata.AudioCodec)));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("AudioPayloadType: {}\r\n\r\n", metadata.AudioPayloadType));
    mockTransportPtr->InjectReceivedBytes(
        fmt::format("AudioIngestSSRC: {}\r\n\r\n", metadata.AudioSsrc));

    // Connect
    mockTransportPtr->InjectReceivedBytes(".\r\n\r\n");

    // Verify that a media port has been requested and metadata has been reported correctly
    controlState = GetFtlControlConnectionState(controlConnection.get());
    REQUIRE(controlState.has_value());
    REQUIRE(controlState.value().MediaPortRequest.has_value());
    REQUIRE(controlState.value().MediaPortRequest.value().ChannelId == channelId);
    MediaMetadata requestMetadata = controlState.value().MediaPortRequest.value().MediaMetadataInfo;
    REQUIRE(requestMetadata.VendorName == metadata.VendorName);
    REQUIRE(requestMetadata.VendorVersion == metadata.VendorVersion);
    REQUIRE(requestMetadata.HasVideo == metadata.HasVideo);
    REQUIRE(requestMetadata.HasAudio == metadata.HasAudio);
    REQUIRE(requestMetadata.VideoCodec == metadata.VideoCodec);
    REQUIRE(requestMetadata.AudioCodec == metadata.AudioCodec);
    REQUIRE(requestMetadata.VideoWidth == metadata.VideoWidth);
    REQUIRE(requestMetadata.VideoHeight == metadata.VideoHeight);
    REQUIRE(requestMetadata.VideoSsrc == metadata.VideoSsrc);
    REQUIRE(requestMetadata.AudioSsrc == metadata.AudioSsrc);
    REQUIRE(requestMetadata.VideoPayloadType == metadata.VideoPayloadType);
    REQUIRE(requestMetadata.AudioPayloadType == metadata.AudioPayloadType);

    // Assign a media port
    controlConnection->StartMediaPort(mediaPort);

    // Verify we received a response
    REQUIRE(lastPayloadReceived.size() > 0);
    std::string mediaPortMessage = std::string(
        reinterpret_cast<char*>(lastPayloadReceived.data()),
        (reinterpret_cast<char*>(lastPayloadReceived.data()) + lastPayloadReceived.size()));
    std::string expectedMediaPortMessage = fmt::format("{} hi. Use UDP port {}\n", 200, mediaPort);
    REQUIRE(mediaPortMessage == expectedMediaPortMessage);
}