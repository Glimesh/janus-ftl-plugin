/**
 * @file IngestConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "CredStore.h"

#include <thread>
#include <random>
#include <regex>
#include <memory>
#include <map>
#include <functional>

/**
 * @brief Enum value describing the current state of the ingest connection
 */
enum class IngestConnectionState
{
    Pending,       // Someone has connected, but hasn't yet completed the authentication process.
    Authenticated, // Someone has connected and authenticated, but has not completed the handshake.
    Active,        // Handshake has been completed and this connection is active.
    Closed         // Connection has been closed.
};

/**
 * @brief This class manages the FTL ingest connection.
 * 
 */
class IngestConnection
{
public:
    /* Constructor/Destructor */
    IngestConnection(int connectionHandle, std::shared_ptr<CredStore> credStore);

    /* Public methods */
    void Start();
    void Stop();
    // Getters/Setters
    uint32_t GetChannelId();
    // Callbacks
    void SetOnStateChanged(
        std::function<void (IngestConnection&, IngestConnectionState)> callback);
    void SetOnRequestMediaPort(std::function<uint16_t (IngestConnection&)> callback);

private:
    /* Private members */
    bool isAuthenticated = false;
    uint32_t channelId = 0;
    int connectionHandle;
    std::shared_ptr<CredStore> credStore;
    std::thread connectionThread;
    const std::array<char, 4> commandDelimiter = { '\r', '\n', '\r', '\n' };
    std::array<uint8_t, 128> hmacPayload;
    std::default_random_engine randomEngine { std::random_device()() };
    std::map<std::string, std::string> attributes;
    // Regex patterns
    std::regex connectPattern = std::regex(R"~(CONNECT ([0-9]+) \$([0-9a-f]+))~");
    std::regex attributePattern = std::regex(R"~((.+): (.+))~");
    // Callbacks
    std::function<void (IngestConnection&, IngestConnectionState)> onStateChanged;
    std::function<uint16_t (IngestConnection&)> onRequestMediaPort;

    /* Private methods */
    void startConnectionThread();
    // Commands
    void processCommand(std::string command);
    void processHmacCommand();
    void processConnectCommand(std::string command);
    void processAttributeCommand(std::string command);
    void processDotCommand();
    void processPingCommand();
    // Utility methods
    std::string byteArrayToHexString(uint8_t* byteArray, uint32_t length);
    std::vector<uint8_t> hexStringToByteArray(std::string hexString);
};