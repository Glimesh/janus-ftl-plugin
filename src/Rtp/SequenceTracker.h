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
    rtp_extended_sequence_num_t Track(rtp_sequence_num_t seqNum, rtp_timestamp_t timestamp);
    void MarkNackSent(rtp_extended_sequence_num_t extendedSeq);
    std::vector<rtp_extended_sequence_num_t> GetMissing();
    uint64_t GetReceivedCount() const;
    uint64_t GetMissedCount() const;
    uint64_t GetLostCount() const;
    
    friend std::ostream& operator<<(std::ostream & out, const SequenceTracker& self);
    
    static constexpr rtp_sequence_num_t BUFFER_SIZE = 2048;
    static constexpr rtp_sequence_num_t REORDER_DELTA = 16;
    static constexpr std::chrono::milliseconds REORDER_TIMEOUT = 20ms;
    static constexpr size_t MAX_OUTSTANDING_NACKS = 64;
    static constexpr std::chrono::milliseconds NACK_TIMEOUT = 2s;
    static constexpr rtp_sequence_num_t MAX_DROPOUT = ExtendedSequenceCounter::MAX_DROPOUT;

private:
    struct Entry
    {
        rtp_extended_sequence_num_t extendedSeq;
        std::chrono::steady_clock::time_point received_at;
        rtp_timestamp_t timestamp;
    };

    struct OutstandingNack
    {
        rtp_extended_sequence_num_t extendedSeq;
        std::chrono::steady_clock::time_point sent_at;
    };
    
    bool initialized = false;
    std::map<rtp_extended_sequence_num_t, Entry> buffer;
    std::set<rtp_extended_sequence_num_t> missing;
    std::map<rtp_sequence_num_t, OutstandingNack> nacks;

    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t maxSeq = 0;
    rtp_extended_sequence_num_t checkForMissingWatermark = 0;

    // Stats
    uint64_t receivedCount = 0;
    uint64_t missedCount = 0;
    uint64_t packetsSinceLastMissed = 0;
    uint64_t lostCount = 0;

    void trackRetransmit(OutstandingNack nack, rtp_timestamp_t timestamp);
    rtp_extended_sequence_num_t trackNewPacket(rtp_sequence_num_t seq, rtp_timestamp_t timestamp);
    void insert(rtp_extended_sequence_num_t extendedSeq, rtp_timestamp_t timestamp);
    void checkForMissing(rtp_extended_sequence_num_t extendedSeq, rtp_timestamp_t timestamp);
    void checkGap(rtp_extended_sequence_num_t begin, rtp_extended_sequence_num_t end);
    void missedPacket(rtp_extended_sequence_num_t extendedSeq);
    void resync();
};
