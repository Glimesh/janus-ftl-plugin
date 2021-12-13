/**
 * @file SequenceTracker.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-12-08
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#pragma once

#include "ExtendedSequenceCounter.h"
#include "Types.h"

#include <chrono>
#include <list>
#include <map>
#include <set>

using namespace std::literals;

// PacketTracker
class SequenceTracker
{
public:
    rtp_extended_sequence_num_t Track(rtp_sequence_num_t seq);
    bool Emplace(rtp_extended_sequence_num_t seq);
    void NackSent(rtp_extended_sequence_num_t seq);
    std::list<rtp_extended_sequence_num_t> GetMissing() const;
    void Reset();
    
    static constexpr rtp_sequence_num_t REORDER_BUFFER_SIZE = 128;
    static constexpr std::chrono::milliseconds REORDER_BUFFER_TIMEOUT = 20ms;
    static constexpr rtp_sequence_num_t MAX_DROPOUT = ExtendedSequenceCounter::MAX_DROPOUT;
    static constexpr size_t MAX_OUTSTANDING_NACKS = 32;
    static constexpr size_t MAX_MISSING_SET_SIZE = 1024;
    static constexpr size_t MAX_NACKS_OUTSTANDING_SET_SIZE = 1024;
    static constexpr std::chrono::milliseconds NACK_OUTSTANDING_TIMEOUT = 2s;
    static constexpr size_t MAX_RECEIVE_BUFFER_SIZE = 4096;


private:
    ExtendedSequenceCounter counter;
    bool initialized = false;
    rtp_extended_sequence_num_t watermark = 0;
    uint64_t packetsMissed = 0;
    uint64_t packetsSinceLastMissed = 0;
    std::set<rtp_extended_sequence_num_t> received;
    std::map<rtp_extended_sequence_num_t, std::chrono::steady_clock::time_point> reorderBuffer;
    std::set<rtp_extended_sequence_num_t> missing;
    std::map<rtp_extended_sequence_num_t, std::chrono::steady_clock::time_point> nacksOutstanding;

    void checkForMissing(rtp_extended_sequence_num_t seq);
    void missedPacket(rtp_extended_sequence_num_t seq);
};
