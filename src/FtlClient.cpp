/**
 * @file FtlClient.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-14
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "FtlClient.h"

#include "Util.h"

#include <fmt/core.h>
#include <netdb.h>
#include <openssl/hmac.h>
#include <regex>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#pragma region Constructor/Destructor
FtlClient::FtlClient(
    std::string targetHostname,
    ftl_channel_id_t channelId,
    std::vector<std::byte> streamKey) : 
    targetHostname(targetHostname),
    channelId(channelId),
    streamKey(std::move(streamKey))
{ }
#pragma endregion Constructor/Destructor

#pragma region Public methods
Result<void> FtlClient::ConnectAsync(FtlClient::ConnectMetadata metadata)
{
    // Look up hostname
    addrinfo addrHints { 0 };
    addrHints.ai_family = AF_INET; // TODO: IPV6 support
    addrHints.ai_socktype = SOCK_STREAM;
    addrHints.ai_protocol = IPPROTO_TCP;
    addrinfo* addrLookup = nullptr;
    int lookupErr = getaddrinfo(
        targetHostname.c_str(),
        std::to_string(FTL_CONTROL_PORT).c_str(),
        &addrHints,
        &addrLookup);
    if (lookupErr != 0)
    {
        freeaddrinfo(addrLookup);
        return Result<void>::Error("Error looking up hostname");
    }

    // Attempt to open TCP connection
    // TODO: Loop through additional addresses on failure. For now, only try the first one.
    addrinfo targetAddr = *addrLookup;
    controlSocketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int connectErr = connect(controlSocketHandle, targetAddr.ai_addr, targetAddr.ai_addrlen);
    if (connectErr != 0)
    {
        close(controlSocketHandle);
        freeaddrinfo(addrLookup);
        return Result<void>::Error("Could not connect to Orchestration service on given host");
    }

    freeaddrinfo(addrLookup);

    // Start a new thread to read incoming data
    connectionThread = std::thread(&FtlClient::connectionThreadBody, this);
    connectionThread.detach();

    // Request HMAC payload and wait for response
    sendControlMessage("HMAC\r\n\r\n");
    Result<FtlClient::FtlResponse> hmacResponse = waitForResponse();
    if (hmacResponse.IsError)
    {
        Stop();
        return Result<void>::Error("Did not receive response to HMAC payload request.");
    }
    std::string hmacPayload = hmacResponse.Value.payload;

    // Hash payload against stream key
    std::byte buffer[512];
    uint32_t bufferLength;
    HMAC(
        EVP_sha512(),
        streamKey.data(),
        streamKey.size(),
        reinterpret_cast<const unsigned char*>(hmacPayload.c_str()),
        hmacPayload.size(),
        reinterpret_cast<unsigned char*>(buffer),
        &bufferLength);

    // Convert hash to string
    std::string hashString = Util::ByteArrayToHexString(&buffer[0], bufferLength);

    // Send authenticated HMAC request
    // format: `CONNECT %d $%s\r\n\r\n` (%d = channelId, %s = hmac hash hex)
    sendControlMessage(fmt::format("CONNECT {} ${}\r\n\r\n", channelId, hashString));
    Result<FtlClient::FtlResponse> authResponse = waitForResponse();
    if (authResponse.IsError)
    {
        Stop();
        return Result<void>::Error("Did not receive successful response to HMAC authentication.");
    }
    if (authResponse.Value.statusCode != 200)
    {
        Stop();
        return Result<void>::Error("Received error in response to HMAC authentication.");
    }

    // Now let's send all of our metadata.
    sendControlMessage(fmt::format(
        "ProtocolVersion: {}.{}\r\n\r\n",
        FTL_PROTOCOL_VERSION_MAJOR,
        FTL_PROTOCOL_VERSION_MINOR));
    sendControlMessage(fmt::format("VendorName: {}\r\n\r\n", metadata.VendorName));
    sendControlMessage(fmt::format("VendorVersion: {}\r\n\r\n", metadata.VendorVersion));
    sendControlMessage(fmt::format("Video: {}\r\n\r\n", metadata.HasVideo ? "true" : "false"));
    sendControlMessage(fmt::format("VideoCodec: {}\r\n\r\n", metadata.VideoCodec));
    sendControlMessage(fmt::format("VideoHeight: {}\r\n\r\n", metadata.VideoHeight));
    sendControlMessage(fmt::format("VideoWidth: {}\r\n\r\n", metadata.VideoWidth));
    sendControlMessage(fmt::format("VideoPayloadType: {}\r\n\r\n", metadata.VideoPayloadType));
    sendControlMessage(fmt::format("VideoIngestSSRC: {}\r\n\r\n", metadata.VideoIngestSsrc));
    sendControlMessage(fmt::format("Audio: {}\r\n\r\n", metadata.HasAudio ? "true" : "false"));
    sendControlMessage(fmt::format("AudioCodec: {}\r\n\r\n", metadata.AudioCodec));
    sendControlMessage(fmt::format("AudioPayloadType: {}\r\n\r\n", metadata.AudioPayloadType));
    sendControlMessage(fmt::format("AudioIngestSSRC: {}\r\n\r\n", metadata.AudioIngestSsrc));

    // Indicate that we are done providing metadata and wait for a response.
    sendControlMessage(".\r\n\r\n");
    Result<FtlClient::FtlResponse> metadataResponse = waitForResponse();
    if (metadataResponse.IsError)
    {
        Stop();
        return Result<void>::Error("Didn't receive a response after providing stream metadata.");
    }

    // Attempt to parse the port assignment out of the response payload
    std::regex portPattern = std::regex(R"~(Use UDP port ([0-9]+))~", std::regex_constants::icase);
    std::smatch portPatternMatch;
    if (!std::regex_match(metadataResponse.Value.payload, portPatternMatch, portPattern) ||
        (portPatternMatch.size() < 2))
    {
        Stop();
        return Result<void>::Error("Expected a UDP port assignment but didn't receive one.");
    }
    try
    {
        int parsedPort = std::stoi(portPatternMatch[1].str());
        if ((parsedPort < 0) || (parsedPort > UINT16_MAX))
        {
            Stop();
            return Result<void>::Error("Invalid UDP port assignment.");
        }
        assignedMediaPort = static_cast<uint16_t>(parsedPort);
    }
    catch (...)
    {
        Stop();
        return Result<void>::Error("Invalid UDP port assignment.");
    }

    // TODO: Open UDP connection to assigned UDP port

    return Result<void>::Success();
}

void FtlClient::SetOnClosed(std::function<void()> onClosed)
{
    this->onClosed = onClosed;
}
#pragma endregion Public methods

#pragma region Private methods
void FtlClient::connectionThreadBody()
{
    std::string receivedBytes;
    char recvBuffer[512] = {0};
    int readBytes = 0;

    while (true)
    {
        readBytes = read(controlSocketHandle, recvBuffer, sizeof(recvBuffer));

        if (readBytes < 0)
        {
            // TODO: We're closing or something went wrong
            break;
        }

        receivedBytes.insert(receivedBytes.end(), recvBuffer, (recvBuffer + readBytes));
        size_t requestEndPosition = 
            receivedBytes.find("\n", (receivedBytes.size() - readBytes), readBytes);
        if (requestEndPosition != std::string::npos)
        {
            // Pull out the request
            std::string requestStr(
                receivedBytes.begin(),
                (receivedBytes.begin() + requestEndPosition));
            receivedBytes.erase(
                receivedBytes.begin(),
                (receivedBytes.begin() + requestEndPosition));

            // We expect at least a status code plus newline character
            if (requestStr.size() < 4)
            {
                // TODO: bad request.
                break;
            }

            // Parse the status code (3 digits)
            uint16_t statusCode = 0;
            try
            {
                int statusCodeInt = 
                    std::stoi(std::string(requestStr.begin(), requestStr.begin() + 3));
                if ((statusCodeInt >= 0) && (statusCodeInt < UINT16_MAX))
                {
                    statusCode = static_cast<uint16_t>(statusCodeInt);
                }
                else
                {
                    // TODO: bad request.
                    break;
                }
            }
            catch (...)
            {
                // TODO: bad request.
                break;
            }

            // Sometimes there's a space before the payload... sometimes there's not. 🤷‍♂️
            std::string payload;
            if ((receivedBytes.at(3) == ' ') && (receivedBytes.size() > 4))
            {
                payload = std::string((receivedBytes.begin() + 4), (receivedBytes.end() - 1));
            }
            else
            {
                payload = std::string((receivedBytes.begin() + 3), (receivedBytes.end() - 1));
            }

            FtlClient::FtlResponse response
            {
                .statusCode = statusCode,
                .payload = payload,
            };
            {
                std::lock_guard<std::mutex> lock(recvResponseMutex);
                receivedResponses.push(response);
            }
            recvResponseConditionVariable.notify_one();
        }
    }
}

void FtlClient::sendControlMessage(std::string message)
{
    write(controlSocketHandle, message.c_str(), message.size());
}

Result<FtlClient::FtlResponse> FtlClient::waitForResponse(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(recvResponseMutex);
    recvResponseConditionVariable.wait_for(
        lock,
        timeout,
        [this]() { return !receivedResponses.empty(); });
    if (receivedResponses.empty())
    {
        // We've timed out
        return Result<FtlClient::FtlResponse>::Error("Timeout expired waiting for a response.");
    }
    else
    {
        FtlClient::FtlResponse response = receivedResponses.front();
        receivedResponses.pop();
        return Result<FtlClient::FtlResponse>::Success(response);
    }
}
#pragma endregion