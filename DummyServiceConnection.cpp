/**
 * @file DummyServiceConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-09
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "DummyServiceConnection.h"

std::string DummyServiceConnection::GetHmacKey(uint32_t userId)
{
    return "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
}

uint32_t DummyServiceConnection::CreateStream(uint32_t userId)
{
    return 1;
}

void DummyServiceConnection::StartStream(uint32_t streamId)
{ }

void DummyServiceConnection::UpdateStreamMetadata(uint32_t streamId, StreamMetadata metadata)
{ }

void DummyServiceConnection::EndStream(uint32_t streamId)
{ }
