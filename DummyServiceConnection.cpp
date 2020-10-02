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

void DummyServiceConnection::Init()
{ }

std::string DummyServiceConnection::GetHmacKey(ftl_channel_id_t channelId)
{
    return "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
}

ftl_stream_id_t DummyServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    return channelId;
}

void DummyServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata)
{ }

void DummyServiceConnection::EndStream(ftl_stream_id_t streamId)
{ }
