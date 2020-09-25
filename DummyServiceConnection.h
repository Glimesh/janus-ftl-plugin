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
    std::string GetHmacKey(uint32_t userId) override;
    uint32_t CreateStream(uint32_t userId) override;
    void StartStream(uint32_t streamId) override;
    void UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata) override;
    void EndStream(uint32_t streamId) override;
};