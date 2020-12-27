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
#include <sys/stat.h>
#include <sstream>
#include <stdexcept>

#pragma region Constructor/Destructor
DummyServiceConnection::DummyServiceConnection(std::string hmacKey, std::string previewSavePath) : 
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

std::string DummyServiceConnection::GetHmacKey(ftl_channel_id_t channelId)
{
    return this->hmacKey;
}

ftl_stream_id_t DummyServiceConnection::StartStream(ftl_channel_id_t channelId)
{
    return currentStreamId++;
}

void DummyServiceConnection::UpdateStreamMetadata(ftl_stream_id_t streamId, StreamMetadata metadata)
{ }

void DummyServiceConnection::EndStream(ftl_stream_id_t streamId)
{ }

void DummyServiceConnection::SendJpegPreviewImage(
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
        throw ServiceConnectionCommunicationFailedException(errStr.str().c_str());
    }

    jpegFile.write(reinterpret_cast<const char*>(jpegData.data()), jpegData.size());
    jpegFile.close();
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