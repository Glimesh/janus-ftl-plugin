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

    std::vector<rtp_extended_sequence_num_t> GetMissing() const;
    uint64_t GetPacketsLost() const;
    
    friend std::ostream& operator<<(std::ostream & out, const SequenceTracker& self);
    
    static constexpr rtp_sequence_num_t REORDER_BUFFER_SIZE = 16;
    static constexpr std::chrono::milliseconds REORDER_BUFFER_TIMEOUT = 20ms;
    static constexpr rtp_sequence_num_t RECEIVE_BUFFER_SIZE = 2048;
    static constexpr std::chrono::milliseconds RECEIVE_BUFFER_TIMEOUT = 2s;
    static constexpr rtp_sequence_num_t MAX_DROPOUT = ExtendedSequenceCounter::MAX_DROPOUT;
    static constexpr size_t MAX_OUTSTANDING_NACKS = 64;

private:
    ExtendedSequenceCounter counter;
    bool initialized = false;
    rtp_extended_sequence_num_t maxSeq = 0;
    uint64_t packetsReceived = 0;
    uint64_t packetsMissed = 0;
    uint64_t packetsSinceLastMissed = 0;
    uint64_t packetsLost = 0;
    std::map<rtp_extended_sequence_num_t, std::chrono::steady_clock::time_point> reorderBuffer;
    std::map<rtp_extended_sequence_num_t, std::chrono::steady_clock::time_point> receiveBuffer;
    std::set<rtp_extended_sequence_num_t> missing;
    std::map<rtp_extended_sequence_num_t, std::chrono::steady_clock::time_point> nacksOutstanding;
    std::map<rtp_sequence_num_t, rtp_extended_sequence_num_t> nackMapping;

    void checkForMissing(rtp_extended_sequence_num_t seq);
    void missedPacket(rtp_extended_sequence_num_t seq);
    void resync();
};
