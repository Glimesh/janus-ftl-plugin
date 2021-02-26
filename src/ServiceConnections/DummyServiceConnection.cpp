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

#include "../Utilities/FtlTypes.h"

#include <fstream>
#include <iostream>
#include <linux/limits.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

#pragma region Constructor/Destructor
DummyServiceConnection::DummyServiceConnection(std::vector<std::byte> hmacKey,
    std::string previewSavePath)
:
    hmacKey(hmacKey),
    previewSavePath(previewSavePath)
{ }
#pragma endregion

#pragma region Public methods
void DummyServiceConnection::Init()
{
    // Make sure the path we're writing thumbnails to exists
    if (recursiveMkDir(previewSavePath) != 0)
    {
        std::stringstream errStr;
        errStr << "Could not create directory '" << previewSavePath << "' to save thumbnails.";
        throw std::runtime_error(errStr.str());
    }
}

Result<std::vector<std::byte>> DummyServiceConnection::GetHmacKey(ftl_channel_id_t channelId)
{
    return Result<std::vector<std::byte>>::Success(this->hmacKey);
}

Result<ftl_stream_id_t> DummyServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    return Result<ftl_stream_id_t>::Success(currentStreamId++);
}

Result<ServiceConnection::ServiceResponse> DummyServiceConnection::UpdateStreamMetadata(
    ftl_stream_id_t streamId, StreamMetadata metadata)
{
    spdlog::debug("Stats received for stream {}:"
        "\n\tStreamTimeSeconds: {}"
        "\n\tNumActiveViewers: {}"
        "\n\tCurrentSourceBitrateBps: {}"
        "\n\tNumPacketsReceived: {}"
        "\n\tNumPacketsNacked: {}"
        "\n\tNumPacketsLost: {}"
        "\n\tStreamToIngestPingMs: {}"
        "\n\tStreamerClientVendorName: {}"
        "\n\tStreamerClientVendorVersion: {}"
        "\n\tVideoCodec: {}"
        "\n\tAudioCodec: {}"
        "\n\tVideoWidth: {}"
        "\n\tVideoHeight: {}",
        streamId,
        metadata.streamTimeSeconds,
        metadata.numActiveViewers,
        metadata.currentSourceBitrateBps,
        metadata.numPacketsReceived,
        metadata.numPacketsNacked,
        metadata.numPacketsLost,
        metadata.streamerToIngestPingMs,
        metadata.streamerClientVendorName,
        metadata.streamerClientVendorVersion,
        metadata.videoCodec,
        metadata.audioCodec,
        metadata.videoWidth,
        metadata.videoHeight);
    return Result<ServiceResponse>::Success(ServiceResponse::Ok);
}

Result<void> DummyServiceConnection::EndStream(ftl_stream_id_t streamId)
{
    return Result<void>::Success();
}

Result<void> DummyServiceConnection::SendJpegPreviewImage(
    ftl_stream_id_t streamId,
    std::vector<uint8_t> jpegData)
{
    std::stringstream pathStr;
    pathStr << previewSavePath << "/" << streamId << ".jpg";

    std::ofstream jpegFile;
    jpegFile.open(pathStr.str().c_str());
    if (jpegFile.fail())
    {
        std::stringstream errStr;
        errStr << "Could not open file '" << pathStr.str().c_str() << "' for writing.";
        return Result<void>::Error(errStr.str());
    }

    jpegFile.write(reinterpret_cast<const char*>(jpegData.data()), jpegData.size());
    jpegFile.close();
    return Result<void>::Success();
}
#pragma endregion

#pragma Private methods
int DummyServiceConnection::recursiveMkDir(std::string path)
{
    errno = 0;

    // Create every node along the path
    for (size_t i = 1; i < path.size(); ++i)
    {
        if (path.at(i) == '/')
        {
            if (mkdir(path.substr(0, i).c_str(), S_IRWXU) != 0)
            {
                if (errno != EEXIST)
                {
                    return -1; 
                }
            }
        }
    }

    // Create final dir
    if (mkdir(path.c_str(), S_IRWXU) != 0)
    {
        if (errno != EEXIST)
        {
            return -1; 
        }
    }   

    return 0;
}
#pragma endregion