/**
 * @file EdgeNodeServiceConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-18
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "EdgeNodeServiceConnection.h"

#pragma region Constructor/Destructor
EdgeNodeServiceConnection::EdgeNodeServiceConnection()
{ }
#pragma endregion Constructor/Destructor

#pragma region Public methods
std::vector<std::byte> EdgeNodeServiceConnection::ProvisionStreamKey(ftl_channel_id_t channelId)
{
    return std::vector<std::byte>();
}

void EdgeNodeServiceConnection::ClearStreamKey(ftl_channel_id_t channelId)
{

}
#pragma endregion Public methods

#pragma region ServiceConnection
void EdgeNodeServiceConnection::Init()
{

}

std::string EdgeNodeServiceConnection::GetHmacKey(ftl_channel_id_t channelId)
{
    return "";
}

ftl_stream_id_t EdgeNodeServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    return 0;
}

void EdgeNodeServiceConnection::UpdateStreamMetadata(
    ftl_stream_id_t streamId,
    StreamMetadata metadata)
{

}

void EdgeNodeServiceConnection::EndStream(
    ftl_stream_id_t streamId)
{

}

void EdgeNodeServiceConnection::SendJpegPreviewImage(
    ftl_stream_id_t streamId,
    std::vector<uint8_t> jpegData)
{

}
#pragma endregion ServiceConnection