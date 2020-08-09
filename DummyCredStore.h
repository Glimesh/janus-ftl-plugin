/**
 * @file DummyCredStore.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "CredStore.h"

/**
 * @brief
 * DummyCredStore is a generic credential store implementation that returns static
 * values for testing.
 */
class DummyCredStore : 
    public CredStore
{
public:
    std::string GetHmacKey(uint32_t userId) override;
};