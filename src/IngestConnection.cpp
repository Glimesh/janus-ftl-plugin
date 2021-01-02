/**
 * @file IngestConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "IngestConnection.h"

#include "ConnectionTransports/ConnectionTransport.h"
#include "Utilities/Util.h"

#include <arpa/inet.h>
#include <fmt/core.h>
#include <iomanip>
#include <openssl/hmac.h>
#include <optional>
#include <sys/socket.h>
#include <unistd.h>

extern "C"
{
    #include <debug.h>
}

const std::array<char, 4> IngestConnection::commandDelimiter = { '\r', '\n', '\r', '\n' };

#pragma region Constructor/Destructor
IngestConnection::IngestConnection(
    std::unique_ptr<ConnectionTransport> controlConnectionTransport,
    std::shared_ptr<ServiceConnection> serviceConnection) : 
    controlConnectionTransport(std::move(controlConnectionTransport)),
    serviceConnection(serviceConnection)
{ }
#pragma endregion

#pragma region Public methods
Result<void> IngestConnection::Start()
{
    // Bind to control connection events
    controlConnectionTransport->SetOnConnectionClosed(
        std::bind(&IngestConnection::onConnectionTransportClosed, this));
    controlConnectionTransport->SetOnBytesReceived(
        std::bind(
            &IngestConnection::onConnectionTransportBytesReceived,
            this,
            std::placeholders::_1));

    // Attempt to start the connection transport
    Result<void> controlStartResult = controlConnectionTransport->StartAsync();
    if (controlStartResult.IsError)
    {
        return controlStartResult;
    }
    return Result<void>::Success();
}

void IngestConnection::Stop()
{
    // TODO: Try to tell the client nicely that we're outta here
    controlConnectionTransport->Stop();
}

std::optional<sockaddr_in> IngestConnection::GetAddr()
{
    return controlConnectionTransport->GetAddr();
}

std::optional<sockaddr_in6> IngestConnection::GetAddr6()
{
    return controlConnectionTransport->GetAddr6();
}

ftl_channel_id_t IngestConnection::GetChannelId()
{
    return channelId;
}

std::string IngestConnection::GetVendorName()
{
    return vendorName;
}

std::string IngestConnection::GetVendorVersion()
{
    return vendorVersion;
}

bool IngestConnection::GetHasVideo()
{
    return hasVideo;
}

bool IngestConnection::GetHasAudio()
{
    return hasAudio;
}

VideoCodecKind IngestConnection::GetVideoCodec()
{
    return videoCodec;
}

uint16_t IngestConnection::GetVideoWidth()
{
    return videoWidth;
}

uint16_t IngestConnection::GetVideoHeight()
{
    return videoHeight;
}

AudioCodecKind IngestConnection::GetAudioCodec()
{
    return audioCodec;
}

rtp_ssrc_t IngestConnection::GetAudioSsrc()
{
    return audioSsrc;
}

rtp_ssrc_t IngestConnection::GetVideoSsrc()
{
    return videoSsrc;
}

rtp_payload_type_t IngestConnection::GetAudioPayloadType()
{
    return audioPayloadType;
}

rtp_payload_type_t IngestConnection::GetVideoPayloadType()
{
    return videoPayloadType;
}

void IngestConnection::SetOnClosed(std::function<void(void)> callback)
{
    onClosed = callback;
}

void IngestConnection::SetOnRequestMediaConnection(std::function<uint16_t(void)> callback)
{
    onRequestMediaConnection = callback;
}
#pragma endregion

#pragma region Private methods
void IngestConnection::writeStringToControlConnection(const std::string& str)
{
    std::vector<std::byte> writeBytes;
    writeBytes.reserve(str.size());
    for (const char& c : str)
    {
        writeBytes.push_back(static_cast<std::byte>(c));
    }
    controlConnectionTransport->Write(writeBytes);
}

void IngestConnection::onConnectionTransportClosed()
{
    if (onClosed)
    {
        onClosed();
    }
}

void IngestConnection::onConnectionTransportBytesReceived(const std::vector<std::byte>& bytes)
{
    unsigned int delimiterCharactersRead = 0;
    for (int i = 0; i < bytes.size(); ++i)
    {
        char incomingCharacter = static_cast<char>(bytes[i]);
        commandStream << incomingCharacter;
        if (incomingCharacter == commandDelimiter.at(delimiterCharactersRead))
        {
            ++delimiterCharactersRead;
            if (delimiterCharactersRead >= commandDelimiter.size())
            {
                // We've read a command.
                std::string command = commandStream.str();
                command = command.substr(0, command.length() - 4); // strip delimiter
                commandStream.str({});
                commandStream.clear();
                delimiterCharactersRead = 0;
                processCommand(command);
            }
        }
        else
        {
            delimiterCharactersRead = 0;
        }
    }
}

void IngestConnection::processCommand(std::string command)
{
    if (command.compare("HMAC") == 0)
    {
        processHmacCommand();
    }
    else if (command.substr(0,7).compare("CONNECT") == 0)
    {
        processConnectCommand(command);
    }
    else if (std::regex_match(command, attributePattern))
    {
        processAttributeCommand(command);
    }
    else if (command.compare(".") == 0)
    {
        processDotCommand();
    }
    else if (command.substr(0,4).compare("PING") == 0)
    {
        processPingCommand();
    }
    else
    {
        JANUS_LOG(LOG_WARN, "FTL: Unknown ingest command: %s\n", command.c_str());
    }
}

void IngestConnection::processHmacCommand()
{
    // Calculate a new random hmac payload, then send it.
    // We'll need to print it out as a string of hex bytes (00 - ff)
    hmacPayload = Util::GenerateRandomBinaryPayload(HMAC_PAYLOAD_SIZE);
    std::string hmacString = Util::ByteArrayToHexString(
        reinterpret_cast<std::byte*>(&hmacPayload[0]),
        hmacPayload.size());
    JANUS_LOG(LOG_INFO, "FTL: Sending HMAC payload: %s\n", hmacString.c_str());
    writeStringToControlConnection(fmt::format("200 {}\n", hmacString));
}

void IngestConnection::processConnectCommand(std::string command)
{
    std::smatch matches;

    if (std::regex_search(command, matches, connectPattern) &&
        (matches.size() >= 3))
    {
        std::string channelIdStr = matches[1].str();
        std::string hmacHashStr = matches[2].str();

        uint32_t channelId = static_cast<uint32_t>(std::stoul(channelIdStr));
        std::vector<std::byte> hmacHash = Util::HexStringToByteArray(hmacHashStr);

        std::string key = serviceConnection->GetHmacKey(channelId);

        std::byte buffer[512];
        uint32_t bufferLength;
        HMAC(
            EVP_sha512(),
            key.c_str(),
            key.length(),
            reinterpret_cast<const unsigned char*>(hmacPayload.data()),
            hmacPayload.size(),
            reinterpret_cast<unsigned char*>(buffer),
            &bufferLength);

        JANUS_LOG(
            LOG_INFO,
            "FTL: Client hash: %s\n",
            hmacHashStr.c_str());
        JANUS_LOG(
            LOG_INFO,
            "FTL: Server hash: %s\n",
            Util::ByteArrayToHexString(&buffer[0], bufferLength).c_str());

        // Do the hashed values match?
        bool match = true;
        if (bufferLength != hmacHash.size())
        {
            match = false;
        }
        else
        {
            for (unsigned int i = 0; i < bufferLength; ++i)
            {
                if (hmacHash.at(i) != buffer[i])
                {
                    match = false;
                    break;
                }
            }
        }

        if (match)
        {
            JANUS_LOG(LOG_INFO, "FTL: Hashes match!\n");
            writeStringToControlConnection("200\n");
            this->channelId = channelId;
            isAuthenticated = true;
        }
        else
        {
            JANUS_LOG(LOG_INFO, "FTL: Hashes do not match!\n");
            // TODO: Handle error, disconnect
        }
    }
    else
    {
        // TODO: Handle error, disconnect client
        JANUS_LOG(LOG_INFO, "FTL: Malformed CONNECT request!\n");
    }
}

void IngestConnection::processAttributeCommand(std::string command)
{
    if (isStreaming)
    {
        // We're already streaming, can't change attributes now.
        // TODO: Send feedback to client.
        JANUS_LOG(LOG_WARN, "FTL: Client attempted to update attributes after stream started.");
        return;
    }

    std::smatch matches;

    if (std::regex_match(command, matches, attributePattern) &&
        matches.size() >= 3)
    {
        std::string key = matches[1].str();
        std::string value = matches[2].str();

        JANUS_LOG(LOG_INFO, "FTL: Updated attribute `%s`: `%s`\n", key.c_str(), value.c_str());

        if (key.compare("VendorName") == 0)
        {
            vendorName = value;
        }
        else if (key.compare("VendorVersion") == 0)
        {
            vendorVersion = value;
        }
        else if (key.compare("Video") == 0)
        {
            hasVideo = (value.compare("true") == 0);
        }
        else if (key.compare("Audio") == 0)
        {
            hasAudio = (value.compare("true") == 0);
        }
        else if (key.compare("VideoCodec") == 0)
        {
            videoCodec = SupportedVideoCodecs::ParseVideoCodec(value);
        }
        else if (key.compare("AudioCodec") == 0)
        {
            audioCodec = SupportedAudioCodecs::ParseAudioCodec(value);
        }
        else if (key.compare("VideoWidth") == 0)
        {
            // TODO: Handle exceptions and kill the connection
            videoWidth = std::stoul(value);
        }
        else if (key.compare("VideoHeight") == 0)
        {
            // TODO: Handle exceptions and kill the connection
            videoHeight = std::stoul(value);
        }
        else if (key.compare("VideoIngestSSRC") == 0)
        {
            // TODO: Handle exceptions and kill the connection
            videoSsrc = std::stoul(value);
        }
        else if (key.compare("AudioIngestSSRC") == 0)
        {
            // TODO: Handle exceptions and kill the connection
            audioSsrc = std::stoul(value);
        }
        else if (key.compare("VideoPayloadType") == 0)
        {
            // TODO: Handle exceptions and kill the connection
            videoPayloadType = std::stoul(value);
        }
        else if (key.compare("AudioPayloadType") == 0)
        {
            // TODO: Handle exceptions and kill the connection
            audioPayloadType = std::stoul(value);
        }
    }
}

void IngestConnection::processDotCommand()
{
    // Validate our state before we fire up a stream
    if (!isAuthenticated)
    {
        JANUS_LOG(LOG_WARN, "FTL: Attempted handshake without valid authentication.\n");
        // TODO: Throw error and kill connection
        return;
    }
    else if (!hasAudio && !hasVideo)
    {
        JANUS_LOG(LOG_WARN, "FTL: Stream requires at least one audio and/or video stream.\n");
        // TODO: Throw error and kill connection
        return;
    }
    else if (hasAudio && 
        (audioPayloadType == 0 || audioSsrc == 0 || audioCodec == AudioCodecKind::Unsupported))
    {
        JANUS_LOG(LOG_WARN, "FTL: Audio stream requires AudioPayloadType, AudioIngestSSRC,"
            " and valid AudioCodec.\n");
        // TODO: Throw error and kill connection
        return;
    }
    else if (hasVideo && 
        (videoPayloadType == 0 || videoSsrc == 0 || videoCodec == VideoCodecKind::Unsupported))
    {
        JANUS_LOG(LOG_WARN, "FTL: Video stream requires VideoPayloadType, VideoIngestSSRC,"
            " and valid VideoCodec.\n");
        // TODO: Throw error and kill connection
        return;
    }
    else if (onRequestMediaConnection == nullptr)
    {
        JANUS_LOG(LOG_ERR, "FTL: Ingest connection cannot request media ports.\n");
        // TODO: Throw error and kill connection
        return;
    }

    uint16_t assignedPort = onRequestMediaConnection();
    isStreaming = true;
    writeStringToControlConnection(fmt::format("200 hi. Use UDP port {}\n", assignedPort));
}

void IngestConnection::processPingCommand()
{
    writeStringToControlConnection("201\n");
}
#pragma endregion