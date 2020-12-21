/**
 * @file FtlStreamStore.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-20
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "FtlTypes.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

class FtlClient;
class FtlStream;
class JanusSession;

/**
 * @brief Handles storage and retrieval of FTL stream instances and associated viewer sessions
 */
class FtlStreamStore
{
public:
    /* Nested structs */
    /**
     * @brief
     *  Used to manage the state of a relay between an incoming FTL stream and another node.
     *  If this relay is active, FtlClient will be set to an instance of an FtlClient used to relay
     *  stream data.
     *  Otherwise, this is used to indicate that a relay is requested for a particular channel.
     */
    struct RelayStore
    {
        ftl_channel_id_t ChannelId;
        std::string TargetHost;
        std::vector<std::byte> StreamKey;
        std::shared_ptr<FtlClient> FtlClientInstance;
    };

    /**
     * @brief Adds the specified FtlStream instance to the stream store.
     * @param ftlStream The stream to add
     */
    void AddStream(std::shared_ptr<FtlStream> ftlStream);

    /**
     * @brief Adds the specified JanusSession as a viewer of the specified FtlStream
     * @param ftlStream The stream to be viewed
     * @param session The viewer session
     */
    void AddViewer(std::shared_ptr<FtlStream> ftlStream, std::shared_ptr<JanusSession> session);

    /**
     * @brief Returns whether the given FtlStream is present in the stream store.
     * @param ftlStream The stream to be tested
     * @return true if stream is present
     * @return false if stream is not present
     */
    bool HasStream(std::shared_ptr<FtlStream> ftlStream);

    /**
     * @brief Get the FtlStream with the given channel id.
     * @param channelId channel ID to match against FtlStream
     * @return std::shared_ptr<FtlStream> Matching FtlStream
     * @return nullptr No matching FtlStream
     */
    std::shared_ptr<FtlStream> GetStreamByChannelId(uint64_t channelId);

    /**
     * @brief Get the FtlStream with the given media port assignment.
     * @param mediaPort media port to match against FtlStream
     * @return std::shared_ptr<FtlStream> Matching FtlStream
     * @return nullptr No matching FtlStream
     */
    std::shared_ptr<FtlStream> GetStreamByMediaPort(uint16_t mediaPort);

    /**
     * @brief Get the FtlStream currently being viewed by the given session.
     * @param session session to match against FtlStream viewers
     * @return std::shared_ptr<FtlStream> Matching FtlStream
     * @return nullptr No matching FtlStream
     */
    std::shared_ptr<FtlStream> GetStreamBySession(std::shared_ptr<JanusSession> session);

    /**
     * @brief Removes the given stream from the stream store.
     * @param ftlStream The stream to remove
     */
    void RemoveStream(std::shared_ptr<FtlStream> ftlStream);

    /**
     * @brief Removes the given session from viewership of an active FtlStream.
     * @param ftlStream The stream to remove the session from
     * @param session The session to remove
     */
    void RemoveViewer(std::shared_ptr<FtlStream> ftlStream, std::shared_ptr<JanusSession> session);

    /**
     * @brief 
     *  Adds the given session as a 'pending' viewer for a channel ID that does not currently
     *  have an active stream.
     * @param channelId The channel ID the session is pending viewership of
     * @param session The viewer session
     */
    void AddPendingViewerForChannelId(uint16_t channelId, std::shared_ptr<JanusSession> session);

    /**
     * @brief Get the pending viewer sessions for a given channel ID
     * 
     * @param channelId Channel ID to retrieve pending viewers for
     * @return std::list<std::shared_ptr<JanusSession>> List of pending viewers
     */
    std::set<std::shared_ptr<JanusSession>> GetPendingViewersForChannelId(uint16_t channelId);

    /**
     * @brief Return the channel ID if a given session is pending viewership.
     */
    std::optional<ftl_channel_id_t> GetPendingChannelIdForSession(
        std::shared_ptr<JanusSession> session);

    /**
     * @brief Clears pending viewer store for a given channel ID
     * @param channelId Channel ID to clear pending viewers for
     * @return std::list<std::shared_ptr<JanusSession>> List of pending viewers that were cleared
     */
    std::set<std::shared_ptr<JanusSession>> ClearPendingViewersForChannelId(uint16_t channelId);

    /**
     * @brief Removes any pending viewership for a given session.
     * @param session Session to remove from pending viewer stores.
     */
    void RemovePendingViewershipForSession(std::shared_ptr<JanusSession> session);

    /**
     * @brief Adds a relay associated with a channel ID
     */
    void AddRelay(RelayStore relay);

    /**
     * @brief Retrieves a relay for a given channel ID if one exists.
     */
    std::optional<RelayStore> GetRelayForChannelId(ftl_channel_id_t channelId);

    /**
     * @brief Removes references to any relays associated with the given channel ID
     */
    void ClearRelay(ftl_channel_id_t channelId);
private:
    /* Private members */
    std::mutex channelIdMapMutex;
    std::map<uint64_t, std::shared_ptr<FtlStream>> ftlStreamsByChannelId;
    std::mutex mediaPortMapMutex;
    std::map<uint16_t, std::shared_ptr<FtlStream>> ftlStreamsByMediaPort;
    std::mutex sessionMapMutex;
    std::map<std::shared_ptr<JanusSession>, std::shared_ptr<FtlStream>> ftlStreamsBySession;
    std::mutex pendingSessionMutex;
    std::map<uint16_t, std::set<std::shared_ptr<JanusSession>>> pendingChannelIdSessions;
    std::map<std::shared_ptr<JanusSession>, uint16_t> pendingSessionChannelId;
    std::mutex relaysMutex;
    std::map<ftl_channel_id_t, RelayStore> relaysByChannelId;
};