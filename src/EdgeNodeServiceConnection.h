/**
 * @file EdgeNodeServiceConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-18
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "ServiceConnection.h"

#include <unordered_map>

/**
 * @brief
 *  A virtual service connection meant for edge nodes that serve as relays for existing streams.
 *  The EdgeNodeServiceConnection will generate and maintain dynamic stream keys for relaying.
 *  The ingest node is responsible for reporting stream information to an actual service.
 */
class EdgeNodeServiceConnection : public ServiceConnection
{
public:
    /* Constructor/Destructor */
    EdgeNodeServiceConnection();

    /* Public methods */
    /**
     * @brief Generates and stores a temporary stream key for the given channel.
     */
    std::vector<std::byte> ProvisionStreamKey(ftl_channel_id_t channelId);

    /**
     * @brief Clears stored temporary stream key for the given channel.
     */
    void ClearStreamKey(ftl_channel_id_t channelId);

    /* ServiceConnection */
    void Init() override;
    std::string GetHmacKey(ftl_channel_id_t channelId) override;
    ftl_stream_id_t StartStream(ftl_channel_id_t channelId) override;
    void UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata) override;
    void EndStream(ftl_stream_id_t streamId) override;
    void SendJpegPreviewImage(ftl_stream_id_t streamId, std::vector<uint8_t> jpegData) override;

private:
    std::unordered_map<ftl_channel_id_t, std::vector<std::byte>> streamKeys;
};