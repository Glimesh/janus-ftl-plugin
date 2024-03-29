/**
 * @file SequenceTracker.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include "SequenceTracker.h"

#include <spdlog/spdlog.h>

#pragma region Public methods

rtp_extended_sequence_num_t SequenceTracker::Track(rtp_sequence_num_t seqNum)
{
    // Check if this is a re-transmitted packet
    auto it = nacks.find(seqNum);
    if (it != nacks.end())
    {
        OutstandingNack nack = it->second;
        nacks.erase(it);
        trackRetransmit(nack);
        return nack.extendedSeq;
    }

    return trackNewPacket(seqNum);
}

void SequenceTracker::MarkNackSent(rtp_extended_sequence_num_t extendedSeq)
{
    nackCount++;
    nacks.emplace(static_cast<rtp_sequence_num_t>(extendedSeq), OutstandingNack {
        .extendedSeq = extendedSeq,
        .sent_at = std::chrono::steady_clock::now(),
    });
}

std::vector<rtp_extended_sequence_num_t> SequenceTracker::GetNackList()
{
    // If we might exceed the maximum number of outstanding NACKs
    if (missing.size() + nacks.size() >= MAX_OUTSTANDING_NACKS)
    {
        // Then timeout older NACKs the sender failed to retransmit. This gives us an accurate
        // count of how many more NACKs can be sent with the current limits.
        auto now = std::chrono::steady_clock::now();
        for (auto it = nacks.begin(); it != nacks.end();)
        {
            if (now - it->second.sent_at >= NACK_TIMEOUT)
            {
                missing.erase(it->first);
                it = nacks.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // Build a list of missing packets not already NACK'd, starting with the latest missing
    std::vector<rtp_extended_sequence_num_t> toNack;
    for (
        auto it = missing.rbegin();
        it != missing.rend() && toNack.size() + nacks.size() < MAX_OUTSTANDING_NACKS;
        ++it)
    {
        if (!nacks.contains(*it))
        {
            toNack.emplace_back(*it);
        }
    }

    return toNack;
}

uint64_t SequenceTracker::GetReceivedCount() const
{
    return receivedCount;
}

uint64_t SequenceTracker::GetMissedCount() const
{
    return missedCount;
}

uint64_t SequenceTracker::GetNackCount() const
{
    return missedCount;
}

uint64_t SequenceTracker::GetLostCount() const
{
    return lostCount;
}

std::ostream &operator<<(std::ostream &os, const SequenceTracker &self)
{
    os << "SequenceTracker { "
       << "initialized:" << self.initialized << ", "
       << "highestReceived:" << self.highestReceived << ", "
       << "highestChecked:" << self.highestChecked << ", "
       << "buffer.size:" << self.buffer.size() << ", "
       << "missing.size:" << self.missing.size() << ", "
       << "nacks.size:" << self.nacks.size() << ", "
       << "received:" << self.receivedCount << ", "
       << "missed:" << self.missedCount << ", "
       << "lost:" << self.lostCount << ", "
       << "sinceLastMissed:" << self.sinceLastMissed << ", "
       << self.counter << " }";
    return os;
}

#pragma endregion Public methods

#pragma region Private methods

void SequenceTracker::trackRetransmit(OutstandingNack nack)
{
    auto now = std::chrono::steady_clock::now();
    auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(now - nack.sent_at);

    spdlog::trace("Re-transmit of NACK'd packet: seq:{}, delay:{}ms",
                  nack.extendedSeq, delay.count());

    lostCount--;
    insert(nack.extendedSeq);
}

rtp_extended_sequence_num_t SequenceTracker::trackNewPacket(rtp_sequence_num_t seqNum)
{
    auto extendResult = counter.Extend(seqNum);

    if (extendResult.resync)
    {
        spdlog::trace("Resyncing sequence number tracking for source");

        resync();
    }

    if (!extendResult.valid)
    {
        spdlog::trace("Source is not valid, but using RTP packet anyways; seq:{} extended:{}",
                      seqNum, extendResult.extendedSeq);
    }

    insert(extendResult.extendedSeq);
    checkForMissing(extendResult.extendedSeq);

    return extendResult.extendedSeq;
}

void SequenceTracker::insert(rtp_extended_sequence_num_t extendedSeq)
{
    if (buffer.contains(extendedSeq))
    {
        spdlog::trace("Duplicate packet received, nothing to do; extendedSeq:{}",
                      extendedSeq);
        return;
    }

    auto now = std::chrono::steady_clock::now();

    // Drop an older entry to make space if necessary
    if (buffer.size() >= BUFFER_SIZE)
    {
        auto entry = buffer.begin();
        missing.erase(entry->second.extendedSeq);
        nacks.erase(entry->second.extendedSeq);
        buffer.erase(entry);
    }

    // Insert new entry
    missing.erase(extendedSeq);
    nacks.erase(static_cast<rtp_sequence_num_t>(extendedSeq));
    buffer.emplace(extendedSeq, Entry{
                                    .extendedSeq = extendedSeq,
                                    .received_at = now,
                                });
    receivedCount++;
}

void SequenceTracker::checkForMissing(rtp_extended_sequence_num_t extendedSeq)
{
    if (!initialized)
    {
        highestChecked = extendedSeq;
        initialized = true;
    }

    if (extendedSeq > highestReceived)
    {
        highestReceived = extendedSeq;
    }

    rtp_extended_sequence_num_t lowerBound = highestChecked + 1;
    rtp_extended_sequence_num_t upperBound =
        highestReceived > REORDER_DELTA ? highestReceived - REORDER_DELTA : 0;

    if (upperBound <= lowerBound)
    {
        // Nothing to do
        return;
    }

    // Check items that just came out of the reorder "buffer"
    rtp_extended_sequence_num_t lastExtendedSeq = highestChecked;
    for (auto it = buffer.lower_bound(lowerBound); it != buffer.upper_bound(upperBound); ++it)
    {
        checkGap(lastExtendedSeq, it->second.extendedSeq);
        lastExtendedSeq = it->second.extendedSeq;
    }

    // Final gap check
    checkGap(lastExtendedSeq, upperBound);
    
    highestChecked = upperBound - 1;
}

void SequenceTracker::checkGap(rtp_extended_sequence_num_t begin, rtp_extended_sequence_num_t end)
{
    int64_t delta = end - begin;
    if (delta == 1)
    {
        // In-order packet
        sinceLastMissed += 1;
    }
    else if (delta < 0)
    {
        spdlog::trace("Out of order packet with gap of {}, no NACKing; begin:{}, highestChecked:{}",
                      delta, begin, highestChecked);
    }
    else if (delta > MAX_DROPOUT)
    {
        spdlog::warn("Missed {} packets, not NACKing; begin:{}, highestChecked:{}",
                     delta, begin, highestChecked);
    }
    else
    {
        // Mark all sequence numbers in gap as missing (if any)
        for (rtp_extended_sequence_num_t s = begin + 1; s < end; ++s)
        {
            missedPacket(s);
        }
    }
}

void SequenceTracker::missedPacket(rtp_extended_sequence_num_t extendedSeq)
{
    missing.emplace(extendedSeq);
    missedCount += 1;
    lostCount += 1;
    sinceLastMissed = 0;
}

void SequenceTracker::resync()
{
    initialized = false;
    buffer.clear();
    missing.clear();
    nacks.clear();
    highestReceived = 0;
    highestChecked = 0;
    sinceLastMissed = 0;
}

#pragma endregion Private methods
