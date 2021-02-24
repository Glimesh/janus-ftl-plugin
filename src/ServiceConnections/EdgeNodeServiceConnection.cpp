/**
 * @file EdgeNodeServiceConnection.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-18
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "EdgeNodeServiceConnection.h"

#include "../Utilities/Util.h"

// #include <algorithm>

#pragma region Constructor/Destructor
EdgeNodeServiceConnection::EdgeNodeServiceConnection()
{ }
#pragma endregion Constructor/Destructor

#pragma region Public methods
std::vector<std::byte> EdgeNodeServiceConnection::ProvisionStreamKey(ftl_channel_id_t channelId)
{
    // If a stream key already exists for the given channel, just return the existing one
    if (streamKeys.count(channelId) > 0)
    {
        return streamKeys[channelId];
    }
    std::vector<std::byte> newKey = Util::GenerateRandomBinaryPayload(streamKeySize);
    streamKeys[channelId] = newKey;
    return newKey;
}

void EdgeNodeServiceConnection::ClearStreamKey(ftl_channel_id_t channelId)
{
    streamKeys.erase(channelId);
}
#pragma endregion Public methods

#pragma region ServiceConnection
void EdgeNodeServiceConnection::Init()
{ }

Result<std::vector<std::byte>> EdgeNodeServiceConnection::GetHmacKey(ftl_channel_id_t channelId)
{
    if (streamKeys.count(channelId) > 0)
    {
        const auto& key = streamKeys[channelId];
        return Result<std::vector<std::byte>>::Success(key);
    }
    return Result<std::vector<std::byte>>::Error("Could not find key for given channel.");
}

Result<ftl_stream_id_t> EdgeNodeServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    return Result<ftl_stream_id_t>::Success(lastAssignedStreamId++);
}

Result<void> EdgeNodeServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId,
    StreamMetadata metadata)
{
    return Result<void>::Success();
}

Result<void> EdgeNodeServiceConnection::EndStream(ftl_stream_id_t streamId)
{
    return Result<void>::Success();
}

Result<void> EdgeNodeServiceConnection::SendJpegPreviewImage(ftl_stream_id_t streamId,
    std::vector<uint8_t> jpegData)
{
    return Result<void>::Success();
}
#pragma endregion ServiceConnection