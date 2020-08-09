/**
 * @file CredStore.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <string>

/**
 * @brief CredStore is a generic interface for fetching credential information.
 */
class CredStore
{
public:
    /**
     * @brief Get the private HMAC key for a given user ID
     * 
     * @param userId The user ID to fetch the key for
     * @return std::string The HMAC key
     */
    virtual std::string GetHmacKey(uint32_t userId) = 0;
};
