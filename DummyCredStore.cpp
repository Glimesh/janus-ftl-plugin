/**
 * @file DummyCredStore.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "DummyCredStore.h"

std::string DummyCredStore::GetHmacKey(uint32_t userId)
{
    return "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
}