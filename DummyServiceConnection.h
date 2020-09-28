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

/**
 * @brief
 * DummyServiceConnection is a generic service connection implementation that returns static
 * values for testing.
 */
class DummyServiceConnection : 
    public ServiceConnection
{
public:
    void Init() override;
    std::string GetHmacKey(ftl_channel_id_t channelId) override;
    ftl_stream_id_t StartStream(ftl_channel_id_t channelId) override;
    void UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata) override;
    void EndStream(ftl_stream_id_t streamId) override;
};