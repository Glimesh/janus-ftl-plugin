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
     * @brief Get the private HMAC key for a given user ID
     * 
     * @param userId The user ID to fetch the key for
     * @return std::string The HMAC key
     */
    virtual std::string GetHmacKey(uint32_t userId) = 0;

    /**
     * @brief Create a new Stream on the service for the given user ID
     * 
     * @param userId the user to create the stream for
     * @return uint32_t the ID for the created stream
     */
    virtual uint32_t CreateStream(uint32_t userId) = 0;

    /**
     * @brief Marks the given stream ID as live on the service
     * 
     * @param streamId ID of stream to start
     */
    virtual void StartStream(uint32_t streamId) = 0;

    /**
     * @brief Updates the service with additional metadata about a stream
     * 
     * @param streamId ID of stream to update
     * @param metadata metadata of stream
     */
    virtual void UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata) = 0;

    /**
     * @brief Marks the given stream ID as ended on the service
     * 
     * @param streamId ID of stream to end
     */
    virtual void EndStream(uint32_t streamId) = 0;
};
