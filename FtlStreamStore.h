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

#include "FtlStream.h"
#include "JanusSession.h"

#include <memory>
#include <map>
#include <mutex>

/**
 * @brief Handles storage and retrieval of FTL stream instances and their viewer sessions
 */
class FtlStreamStore
{
public:
    /* Public methods */
    void AddStream(std::shared_ptr<FtlStream> ftlStream);
    void AddViewer(std::shared_ptr<FtlStream> ftlStream, std::shared_ptr<JanusSession> session);
    bool HasStream(std::shared_ptr<FtlStream> ftlStream);
    std::shared_ptr<FtlStream> GetStreamByChannelId(uint64_t channelId);
    std::shared_ptr<FtlStream> GetStreamByMediaPort(uint16_t mediaPort);
    std::shared_ptr<FtlStream> GetStreamBySession(std::shared_ptr<JanusSession> session);
    void RemoveStream(std::shared_ptr<FtlStream> ftlStream);
    void RemoveViewer(std::shared_ptr<FtlStream> ftlStream, std::shared_ptr<JanusSession> session);
private:
    /* Private members */
    std::mutex channelIdMapMutex;
    std::map<uint64_t, std::shared_ptr<FtlStream>> ftlStreamsByChannelId;
    std::mutex mediaPortMapMutex;
    std::map<uint16_t, std::shared_ptr<FtlStream>> ftlStreamsByMediaPort;
    std::mutex sessionMapMutex;
    std::map<std::shared_ptr<JanusSession>, std::shared_ptr<FtlStream>> ftlStreamsBySession;
};