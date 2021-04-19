/**
 * @file ControlConnectionManager.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include <arpa/inet.h>

#include "Utilities/FtlTypes.h"

// Forward declarations
class FtlControlConnection;

/**
 * @brief A simple interface used for objects to handle instances of FtlControlConnection
 */
class FtlControlConnectionManager
{
public:
    virtual ~FtlControlConnectionManager() = default;

    /**
     * @brief Called by FtlControlConnection when the control connection has stopped.
     */
    virtual void ControlConnectionStopped(FtlControlConnection* connection) = 0;

    /**
     * @brief Called by FtlControlConnection when it wants an HMAC key for a channel
     */
    virtual void ControlConnectionRequestedHmacKey(FtlControlConnection* connection,
        ftl_channel_id_t channelId) = 0;

    /**
     * @brief Called by FtlControlConnection when it needs a media port assigned
     */
    virtual void ControlConnectionRequestedMediaPort(FtlControlConnection* connection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr) = 0;

};