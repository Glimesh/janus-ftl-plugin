/**
 * @file MockFtlServer.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "../../src/FtlControlConnectionManager.h"

class MockFtlControlConnectionManager : public FtlControlConnectionManager
{
public:
    /* Constructor/Destructor */
    MockFtlControlConnectionManager()
    { }

    /* FtlControlConnectionManager implementation */
    virtual void ControlConnectionStopped(FtlControlConnection* connection)
    {

    }

    virtual void ControlConnectionRequestedHmacKey(FtlControlConnection* connection,
        ftl_channel_id_t channelId)
    {

    }

    virtual void ControlConnectionRequestedMediaPort(FtlControlConnection* connection,
        ftl_channel_id_t channelId, MediaMetadata mediaMetadata, in_addr targetAddr)
    {

    }
};