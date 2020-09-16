/**
 * @file GlimeshServiceConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-15
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <string>
#include "ServiceConnection.h"

/**
 * @brief
 *  GlimeshServiceConnection is a service connection implementation for the Glimesh.tv
 *  platform.
 */
class GlimeshServiceConnection : 
    public ServiceConnection
{
public:
    /* Constructor/Destructor */
    GlimeshServiceConnection(std::string hostname, bool useHttps);

    /* ServiceConnection */
    std::string GetHmacKey(uint32_t userId) override;
    uint32_t CreateStream(uint32_t userId) override;
    void StartStream(uint32_t streamId) override;
    void UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata) override;
    void EndStream(uint32_t streamId) override;

private:
    /* Private members */
    std::string hostname;
    bool useHttps;
};