/**
 * @file FtlClient.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-14
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "FtlTypes.h"
#include "Result.h"

#include <cstdint>
#include <functional>
#include <future>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief A faster-than-light client used to connect to other janus-ftl-plugin instances.
 */
class FtlClient
{
public:
    /* Static public members */
    static constexpr uint16_t FTL_CONTROL_PORT = 8084;

    /* Constructor/Destructor */
    FtlClient(
        std::string targetHostname,
        ftl_channel_id_t channelId,
        std::vector<std::byte> streamKey);
    
    /* Public methods */
    /**
     * @brief Starts FTL connection on a new thread.
     */
    Result<void> ConnectAsync();

    /**
     * @brief Set the callback to be triggered when the connection has been closed.
     */
    void SetOnClosed(std::function<void()> onClosed);

private:
    /* Private members */
    std::string targetHostname;
    ftl_channel_id_t channelId;
    std::vector<std::byte> streamKey;
    std::thread connectionThread;
    // Callbacks
    std::function<void()> onClosed;

    /* Private methods */
    void connectionThreadBody(std::promise<void>&& connectionReadyPromise);
};