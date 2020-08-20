/**
 * @file FtlStreamStore.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-20
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "FtlStreamStore.h"

#include <stdexcept>

#pragma region Public methods
void FtlStreamStore::AddStream(std::shared_ptr<FtlStream> ftlStream)
{
    std::lock_guard<std::mutex> channelIdMapGuard(channelIdMapMutex);
    std::lock_guard<std::mutex> mediaPortMapGuard(mediaPortMapMutex);

    // Check for collisions
    uint64_t channelId = ftlStream->GetChannelId();
    uint16_t mediaPort = ftlStream->GetMediaPort();
    if (ftlStreamsByChannelId.count(channelId) > 0)
    {
        throw std::invalid_argument(
            "Attempt to add FTL Stream with a channel ID that is already assigned.");
    }
    if (ftlStreamsByMediaPort.count(mediaPort) > 0)
    {
        throw std::invalid_argument(
            "Attempt to add FTL Stream with a media port that is already assigned.");
    }

    // Add to maps
    ftlStreamsByChannelId[channelId] = ftlStream;
    ftlStreamsByMediaPort[mediaPort] = ftlStream;
}

void FtlStreamStore::AddViewer(
    std::shared_ptr<FtlStream> ftlStream,
    std::shared_ptr<JanusSession> session)
{
    // First, confirm stream is present
    if (!HasStream(ftlStream))
    {
        throw std::invalid_argument("Could not find given FTL stream.");
    }

    std::lock_guard<std::mutex> sessionMapGuard(sessionMapMutex);

    // Make sure this session isn't already assigned to a different stream
    if (ftlStreamsBySession.count(session) > 0)
    {
        throw std::invalid_argument("This session is already viewing another FTL stream.");
    }

    // Add viewer to ftl stream
    ftlStream->AddViewer(session);
    
    // Update map
    ftlStreamsBySession[session] = ftlStream;
}

bool FtlStreamStore::HasStream(std::shared_ptr<FtlStream> ftlStream)
{
    std::lock_guard<std::mutex> channelIdMapGuard(channelIdMapMutex);
    std::lock_guard<std::mutex> mediaPortMapGuard(mediaPortMapMutex);
    uint64_t channelId = ftlStream->GetChannelId();
    uint64_t mediaPort = ftlStream->GetMediaPort();
    return ((ftlStreamsByChannelId.count(channelId) > 0) &&
            (ftlStreamsByMediaPort.count(mediaPort) > 0));
}

std::shared_ptr<FtlStream> FtlStreamStore::GetStreamByChannelId(uint64_t channelId)
{
    std::lock_guard<std::mutex> channelIdMapGuard(channelIdMapMutex);
    if (ftlStreamsByChannelId.count(channelId) <= 0)
    {
        return nullptr;
    }
    return ftlStreamsByChannelId.at(channelId);
}

std::shared_ptr<FtlStream> FtlStreamStore::GetStreamByMediaPort(uint16_t mediaPort)
{
    std::lock_guard<std::mutex> mediaPortMapGuard(mediaPortMapMutex);
    if (ftlStreamsByMediaPort.count(mediaPort) <= 0)
    {
        return nullptr;
    }
    return ftlStreamsByMediaPort.at(mediaPort);
}

std::shared_ptr<FtlStream> FtlStreamStore::GetStreamBySession(std::shared_ptr<JanusSession> session)
{
    std::lock_guard<std::mutex> sessionMapGuard(sessionMapMutex);
    if (ftlStreamsBySession.count(session) <= 0)
    {
        return nullptr;
    }
    return ftlStreamsBySession.at(session);
}

void FtlStreamStore::RemoveStream(std::shared_ptr<FtlStream> ftlStream)
{
    std::lock_guard<std::mutex> channelIdMapGuard(channelIdMapMutex);
    std::lock_guard<std::mutex> mediaPortMapGuard(mediaPortMapMutex);

    uint64_t channelId = ftlStream->GetChannelId();
    uint16_t mediaPort = ftlStream->GetMediaPort();

    if ((ftlStreamsByChannelId.count(channelId) <= 0) || 
        (ftlStreamsByMediaPort.count(mediaPort) <= 0))
    {
        throw std::invalid_argument("Could not find given stream's channel ID or media port.");
    }

    ftlStreamsByChannelId.erase(channelId);
    ftlStreamsByMediaPort.erase(mediaPort);
}

void FtlStreamStore::RemoveViewer(
    std::shared_ptr<FtlStream> ftlStream,
    std::shared_ptr<JanusSession> session)
{
    // First, confirm stream is present
    if (!HasStream(ftlStream))
    {
        throw std::invalid_argument("Could not find given FTL stream.");
    }

    std::lock_guard<std::mutex> sessionMapGuard(sessionMapMutex);

    // Confirm session exists
    if (ftlStreamsBySession.count(session) <= 0)
    {
        throw std::invalid_argument("Could not find given session.");
    }

    // Confirm session actually points to the expected stream
    if (ftlStreamsBySession.at(session) != ftlStream)
    {
        throw std::invalid_argument("Session is not viewing given stream.");
    }

    // Remove viewer from ftl stream
    ftlStream->RemoveViewer(session);
    
    // Update map
    ftlStreamsBySession.erase(session);
}
#pragma endregion