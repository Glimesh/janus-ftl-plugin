/**
 * @file MockFtlServer.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "../../src/FtlControlConnectionManager.h"

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

class MockFtlControlConnectionManager : public FtlControlConnectionManager
{
public:
    /* Constructor/Destructor */
    MockFtlControlConnectionManager(
        std::function<void(FtlControlConnection*)> onControlConnectionStopped,
        std::function<void(FtlControlConnection*, ftl_channel_id_t)> 
            onControlConnectionRequestedHmacKey,
        std::function<void(FtlControlConnection* connection, ftl_channel_id_t channelId,
            MediaMetadata mediaMetadata, in_addr targetAddr)> onControlConnectionRequestedMediaPort)
    :
        onControlConnectionStopped(onControlConnectionStopped),
        onControlConnectionRequestedHmacKey(onControlConnectionRequestedHmacKey),
        onControlConnectionRequestedMediaPort(onControlConnectionRequestedMediaPort)
    { }

    /* FtlControlConnectionManager implementation */
    virtual void ControlConnectionStopped(FtlControlConnection* connection)
    {
        onControlConnectionStopped(connection);
    }

    virtual void ControlConnectionRequestedHmacKey(FtlControlConnection* connection,
        ftl_channel_id_t channelId)
    {
        onControlConnectionRequestedHmacKey(connection, channelId);
    }

    virtual void ControlConnectionRequestedMediaPort(FtlControlConnection* connection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr)
    {
        onControlConnectionRequestedMediaPort(connection, channelId, mediaMetadata, targetAddr);
    }

private:
    /* Private fields */
    std::function<void(FtlControlConnection*)> onControlConnectionStopped;
    std::function<void(FtlControlConnection*, ftl_channel_id_t)> 
        onControlConnectionRequestedHmacKey;
    std::function<void(FtlControlConnection* connection, ftl_channel_id_t channelId,
        MediaMetadata mediaMetadata, in_addr targetAddr)> onControlConnectionRequestedMediaPort;
};