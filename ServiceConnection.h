/**
 * @file ServiceConnection.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "StreamMetadata.h"
#include "FtlTypes.h"

#include <string>

/**
 * @brief
 *  ServiceConnection is a generic interface for communicating stream information
 *  to a hosted service
 */
class ServiceConnection
{
public:
    virtual ~ServiceConnection()
    { }

    /**
     * @brief Perform any initialization tasks that are required before utilizing the connection
     */
    virtual void Init() = 0;

    /**
     * @brief Get the private HMAC key for a given channel ID
     * 
     * @param channelId The channel ID to fetch the key for
     * @return The HMAC key
     */
    virtual std::string GetHmacKey(ftl_channel_id_t channelId) = 0;

    /**
     * @brief Starts a stream for a given channel
     * 
     * @param channelId ID of channel to start stream on
     * @return The ID of the new stream
     */
    virtual ftl_stream_id_t StartStream(ftl_channel_id_t channelId) = 0;

    /**
     * @brief Updates the service with additional metadata about a stream
     * 
     * @param streamId ID of stream to update
     * @param metadata metadata of stream
     */
    virtual void UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata) = 0;

    /**
     * @brief Marks the given stream ID as ended on the service
     * 
     * @param streamId ID of stream to end
     */
    virtual void EndStream(ftl_stream_id_t streamId) = 0;
};
