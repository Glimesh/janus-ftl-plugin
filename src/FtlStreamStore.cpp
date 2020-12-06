/**
 * @file FtlStreamStore.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-20
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "FtlStreamStore.h"
#include "FtlStream.h"
extern "C"
{
    #include <debug.h>
}

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

void FtlStreamStore::AddPendingViewerForChannelId(
    uint16_t channelId,
    std::shared_ptr<JanusSession> session)
{
    std::lock_guard<std::mutex> lock(pendingSessionMutex);

    // We shouldn't be able to request viewership of multiple channels at once.
    // But make sure anyway.
    if (pendingSessionChannelId.count(session) > 0)
    {
        JANUS_LOG(LOG_WARN, "FTL: Session requesting pending viewership is already pending "
            "viewingship for another channel. Channel ID %d. Re-assigning...",
            channelId);
        uint16_t existingChannelId = pendingSessionChannelId[session];
        if (pendingChannelIdSessions.count(existingChannelId) > 0)
        {
            pendingChannelIdSessions[existingChannelId].erase(session);
        }
        else
        {
            JANUS_LOG(LOG_WARN, "FTL: Session had an existing pending channel ID, but "
                "channel ID set didn't contain session. Stream store is in a bad state.");
        }
    }
    pendingSessionChannelId[session] = channelId;

    if (pendingChannelIdSessions.count(channelId) <= 0)
    {
        pendingChannelIdSessions[channelId] = std::set<std::shared_ptr<JanusSession>>();
    }
    pendingChannelIdSessions[channelId].insert(session);
}

std::set<std::shared_ptr<JanusSession>> FtlStreamStore::GetPendingViewersForChannelId(
    uint16_t channelId)
{
    std::lock_guard<std::mutex> lock(pendingSessionMutex);

    if (pendingChannelIdSessions.count(channelId) > 0)
    {
        return pendingChannelIdSessions[channelId];
    }
    return std::set<std::shared_ptr<JanusSession>>(); // Empty set
}

std::set<std::shared_ptr<JanusSession>> FtlStreamStore::ClearPendingViewersForChannelId(
    uint16_t channelId)
{
    std::lock_guard<std::mutex> lock(pendingSessionMutex);

    // Grab a copy to return
    std::set<std::shared_ptr<JanusSession>> returnVal;
    if (pendingChannelIdSessions.count(channelId) > 0)
    {
        returnVal = pendingChannelIdSessions[channelId];
    }

    for (const std::shared_ptr<JanusSession>& session : returnVal)
    {
        pendingSessionChannelId.erase(session);
    }

    pendingChannelIdSessions.erase(channelId);

    return returnVal;
}

void FtlStreamStore::RemovePendingViewershipForSession(std::shared_ptr<JanusSession> session)
{
    std::lock_guard<std::mutex> lock(pendingSessionMutex);

    // Look up the channel ID
    if (pendingSessionChannelId.count(session) <= 0)
    {
        JANUS_LOG(LOG_WARN, "FTL: Attempt to remove viewership for session that is not pending "
            "any channel ID.");
        return;
    }
    uint16_t channelId = pendingSessionChannelId[session];
    pendingSessionChannelId.erase(session);

    if (pendingChannelIdSessions.count(channelId) <= 0)
    {
        JANUS_LOG(LOG_WARN, "FTL: Session had an existing pending channel ID, but "
            "channel ID set didn't contain session. Stream store is in a bad state.");
        return;
    }
    if (pendingChannelIdSessions[channelId].erase(session) != 1)
    {
        JANUS_LOG(LOG_WARN, "FTL: Failed to erase session from pending channel ID session set.");
    }
}
#pragma endregion