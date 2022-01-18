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

/**
 * @brief Tracks received packets and identifies any missing packets so they can be NACK'd.
 * 
 * This is done by tracking the RTP sequence number of every received packet in a buffer. We then
 * look for "gaps" where sequence numbers are missing. Those numbers can be NACK'd and hopefully
 * the streaming client will re-transmit the missing packets.
 * 
 * A set of carefully tuned parameters control the behavior. In the future we can expand this code
 * to be smarter and rely less on careful tuning. For example, real RTP/WebRTC clients track packet
 * interarrival time in order to estimate when a particular sequence number will arrive. We instead
 * just wait for a newer packet to arrive and say any gaps before that new packet are missing. We do
 * have a small allowance for packet re-ordering, but it is all based on sequence number deltas and
 * not a clock or timestamps.
 */
class SequenceTracker
{
public:
    /* Public methods */
    /**
     * @brief Takes a new or re-transmitted packet and starts tracking it.
     * 
     * All incoming packets from the client should be passed to this method.
     * 
     * @param seqNum Packet's 16-bit RTP sequence number
     * @return rtp_extended_sequence_num_t Extended 64-bit sequence number for the packet.
     */
    rtp_extended_sequence_num_t Track(rtp_sequence_num_t seqNum);
    /**
     * @brief Get the list of sequence numbers to NACK. Once NACK'd, call MarkNackSent
     * 
     * Ordered from latest known missing sequence number to oldest. May not include all missing
     * numbers if the combined count of new missing sequence numbers and existing NACKs are more
     * than MAX_OUTSTANDING_NACKS.
     * 
     * Ideally we would prioritize which packets to NACK based on which were part of a
     * keyframe or other heuristics, but for now the logic is just a simple ordering.
     * 
     * @return std::vector<rtp_extended_sequence_num_t> Ordered vector of missing sequence numbers.
     */
    std::vector<rtp_extended_sequence_num_t> GetNackList();

    /**
     * @brief Tell this tracker a NACK response has been sent for a missing sequence number.
     * 
     * Call GetNackList() to get the list of sequence numbers to NACK, then call this for each. That
     * is an awkward interaction, potentially this code or the NACK sending can be refactored in the
     * future to eliminate the need for this back and forth.
     * 
     * @param extendedSeq The sequence number that a NACK was sent for.
     */
    void MarkNackSent(rtp_extended_sequence_num_t extendedSeq);

    /* Public accessors for packet statistics */
    uint64_t GetReceivedCount() const;
    uint64_t GetMissedCount() const;
    uint64_t GetNackCount() const;
    uint64_t GetLostCount() const;
    
    friend std::ostream& operator<<(std::ostream & out, const SequenceTracker& self);
    
    /* Public constants */
    static constexpr rtp_sequence_num_t BUFFER_SIZE = 2048;

    // Allowance for packet re-ordering. If the packet with sequence number N is arriving late, it
    // will *not* be considered missing and NACK eligible until packet N+REORDER_DELTA has arrived.
    static constexpr rtp_sequence_num_t REORDER_DELTA = 16;

    // Maximum number of packets that can be NACK'd at once. This helps limit NACK floods. For
    // example, say a client with limited bandwidth experiences a dropout and many packets are lost.
    // We do not want to trigger a large bandwidth spike by asking the client to re-transmit all the
    // lost packets.
    static constexpr rtp_sequence_num_t MAX_OUTSTANDING_NACKS = 64;

    // After this long without a successful packet re-transmit, NACKs will be dropped and no longer
    // count against the MAX_OUTSTANDING_NACKS value. Should be tuned based on the maximum expected
    // round trip time (RTT) and frame playout delay. We do not want re-transmitted packets arriving
    // after they are no longer being tracked, but also we do not want to be waiting on re-transmits
    // of packets for frames so old they have been skipped over and will never be rendered.  
    static constexpr std::chrono::milliseconds NACK_TIMEOUT = std::chrono::seconds(2);

    // Maximum number of sequentially missed packets we will sendend NACKs for. If there is a larger
    // gap, it was probably a severe network issue and we should not impose additional bandwidth
    // to do re-transmits, instead just wait for the connection to stabilize.
    static constexpr rtp_sequence_num_t MAX_DROPOUT = ExtendedSequenceCounter::MAX_DROPOUT;

private:
    /**
     * @brief Entry in the tracking buffer
     */
    struct Entry
    {
        rtp_extended_sequence_num_t extendedSeq;
        std::chrono::steady_clock::time_point received_at;
    };

    struct OutstandingNack
    {
        rtp_extended_sequence_num_t extendedSeq;
        std::chrono::steady_clock::time_point sent_at;
    };
    
    std::map<rtp_extended_sequence_num_t, Entry> buffer;
    std::set<rtp_extended_sequence_num_t> missing;
    std::map<rtp_sequence_num_t, OutstandingNack> nacks;

    ExtendedSequenceCounter counter;

    bool initialized = false;

    // Highest sequence number received so far
    rtp_extended_sequence_num_t highestReceived = 0;

    // Highest sequence number checked for missing packets. All sequence numbers lower than this are
    // either in the buffer or are marked as missing (or are old enough no longer be in the buffer).
    rtp_extended_sequence_num_t highestChecked = 0;

    // Packet statistics
    uint64_t receivedCount = 0;
    uint64_t missedCount = 0;
    uint64_t sinceLastMissed = 0;
    uint64_t nackCount = 0;
    // TODO should this be redefined to be permanently lost packets? right now it can tick down when re-transmitted packets for NACKs come in
    uint64_t lostCount = 0;

    void trackRetransmit(OutstandingNack nack);
    rtp_extended_sequence_num_t trackNewPacket(rtp_sequence_num_t seq);
    void insert(rtp_extended_sequence_num_t extendedSeq);
    void checkForMissing(rtp_extended_sequence_num_t extendedSeq);
    void checkGap(rtp_extended_sequence_num_t begin, rtp_extended_sequence_num_t end);
    void missedPacket(rtp_extended_sequence_num_t extendedSeq);
    void resync();
};
