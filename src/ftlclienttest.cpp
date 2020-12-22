/**
 * @file ftlclienttest.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-18
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "FtlClient.h"
#include "Result.h"

#include <iostream>
#include <memory>
#include <vector>

int main()
{
    std::string streamKeyStr("aBcDeFgHiJkLmNoPqRsTuVwXyZ123456");
    std::vector<std::byte> streamKey;
    streamKey.reserve(streamKeyStr.size());
    for (const char& c : streamKeyStr)
    {
        streamKey.push_back(static_cast<std::byte>(c));
    }

    std::unique_ptr<FtlClient> ftlClient = 
        std::make_unique<FtlClient>("localhost", 123456789, streamKey);
    
    Result<void> connectResult = ftlClient->ConnectAsync(FtlClient::ConnectMetadata
    {
        .VendorName = "TEST CLIENT",
        .VendorVersion = "0.0.0",
        .HasVideo = true,
        .VideoCodec = "H264",
        .VideoHeight = 1080,
        .VideoWidth = 1920,
        .VideoPayloadType = 96,
        .VideoIngestSsrc = 123456790,
        .HasAudio = true,
        .AudioCodec = "OPUS",
        .AudioPayloadType = 97,
        .AudioIngestSsrc = 123456789,
    });

    if (connectResult.IsError)
    {
        std::cout << "Error connecting: " << connectResult.ErrorMessage << std::endl;
        return 1;
    }

    std::cout << "Connected. Enter to stop." << std::endl;
    getchar();

    return 0;
}