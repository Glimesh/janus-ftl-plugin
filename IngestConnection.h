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

#include <thread>
#include <random>
#include <regex>

/**
 * @brief This class manages the FTL ingest connection.
 * 
 */
class IngestConnection
{
public:
    /* Constructor/Destructor */
    IngestConnection(int connectionHandle);

    /* Public methods */
    void Start();

private:
    /* Private members */
    int connectionHandle;
    std::thread connectionThread;
    const std::array<char, 4> commandDelimiter = { '\r', '\n', '\r', '\n' };
    std::array<uint8_t, 128> hmacPayload;
    std::default_random_engine randomEngine { std::random_device()() };

    /* Private methods */
    void startConnectionThread();
    void processCommand(std::string command);
    void processHmacCommand();
    void processConnectCommand(std::string command);
};