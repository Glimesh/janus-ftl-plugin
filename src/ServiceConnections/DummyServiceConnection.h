/**
 * @file DummyServiceConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "ServiceConnection.h"

// #include <string>

/**
 * @brief
 * DummyServiceConnection is a generic service connection implementation that returns static
 * values for testing.
 */
class DummyServiceConnection : public ServiceConnection
{
public:
    /* Constructor/Destructor */
    DummyServiceConnection(std::vector<std::byte> hmacKey, std::string previewSavePath);

    // ServiceConnection
    void Init() override;
    Result<std::vector<std::byte>> GetHmacKey(ftl_channel_id_t channelId) override;
    Result<ftl_stream_id_t> StartStream(ftl_channel_id_t channelId) override;
    Result<void> UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata) override;
    Result<void> EndStream(ftl_stream_id_t streamId) override;
    Result<void> SendJpegPreviewImage(ftl_stream_id_t streamId,
        std::vector<uint8_t> jpegData) override;

private:
    std::vector<std::byte> hmacKey;
    std::string previewSavePath;
    ftl_stream_id_t currentStreamId = 0;

    /**
     * @brief 
     *  Recursively creates directories for the given path.
     *  Thanks github.com/JonathonReinhart and http://stackoverflow.com/a/2336245/119527
     * @param path path to directory to recursively create
     * @return int 0 on success, -1 on failure
     */
    int recursiveMkDir(std::string path);
};