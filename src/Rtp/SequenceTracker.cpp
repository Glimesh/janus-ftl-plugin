/**
 * @file SequenceTracker.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include "SequenceTracker.h"

#include <spdlog/spdlog.h>

#pragma region Public methods
rtp_extended_sequence_num_t SequenceTracker::Track(rtp_sequence_num_t seq)
{
    packetsReceived++;
    auto mapping = nackMapping.find(seq);
    if (mapping != nackMapping.end())
    {
        spdlog::trace("Received NACK'd packet: {}", mapping->second);
        packetsLost--;
        Emplace(mapping->second);
        return mapping->second;
    }
    auto extendResult = counter.Extend(seq);

    if (extendResult.resync)
    {
        spdlog::trace("Resyncing sequence number tracking for source");

        resync();
    }

    if (!extendResult.valid)
    {
        spdlog::trace("Source is not valid, but using RTP packet anyways (seq {} (extended to {})",
                      seq, extendResult.extendedSeq);
    }

    Emplace(extendResult.extendedSeq);
    return extendResult.extendedSeq;
}

bool SequenceTracker::Emplace(rtp_extended_sequence_num_t seq)
{
    auto now = std::chrono::steady_clock::now();

    bool inserted = receiveBuffer.emplace(seq, now).second;

    if (!inserted)
    {
        // Duplicate packet received, nothing to do
        return false;
    }

    reorderBuffer.emplace(seq, now);

    // Cleanup reorder buffer by bounded size
    for (auto it = reorderBuffer.begin(); reorderBuffer.size() >= REORDER_BUFFER_SIZE && it != reorderBuffer.end();)
    {
        auto seq = it->first;
        it = reorderBuffer.erase(it);
        checkForMissing(seq);
    }

    // handle any packets that have been sitting too long in reorder buffer
    // TODO replace with jitter based arrival estimator
    for (auto it = reorderBuffer.begin(); it != reorderBuffer.end();)
    {
        if (now - it->second >= REORDER_BUFFER_TIMEOUT)
        {
            auto seq = it->first;
            spdlog::trace("Reorder timeout; seq:{}", seq);
            it = reorderBuffer.erase(it);
            checkForMissing(seq);
        }
        else
        {
            ++it;
        }
    }

    // Cleanup receive buffer
    for (auto it = receiveBuffer.begin(); it != receiveBuffer.end() && receiveBuffer.size() > RECEIVE_BUFFER_SIZE;)
    {
        missing.erase(it->first);
        nacksOutstanding.erase(it->first);
        nackMapping.erase(it->first);
        it = receiveBuffer.erase(it);
    }

    return true;
}

void SequenceTracker::checkForMissing(rtp_extended_sequence_num_t seq)
{
    if (!initialized)
    {
        maxSeq = seq - 1;
        initialized = true;
    }

    missing.erase(seq);
    nacksOutstanding.erase(seq);

    int64_t gap = seq - maxSeq;
    if (gap == 1)
    {
        // In-order packet
        packetsSinceLastMissed += 1;
    }
    else if (gap < 0)
    {
        spdlog::trace("Out of order packet with gap of {}, no NACKing; seq:{}, maxSeq:{}",
                      gap, seq, maxSeq);
    }
    else if (gap > MAX_DROPOUT)
    {
        spdlog::warn("Missed {} packets, not NACKing; seq:{}, maxSeq:{}",
                     gap, seq, maxSeq);
    }
    else
    {
        // Mark all sequence numbers in gap as missing (if any)
        for (int64_t i = 1; i < gap; ++i)
        {
            missedPacket(seq - i);
        }
    }

    if (gap > 0)
    {
        maxSeq = seq;
    }
}

void SequenceTracker::NackSent(rtp_extended_sequence_num_t seq)
{
    nacksOutstanding.emplace(seq, std::chrono::steady_clock::now());
    nackMapping.emplace(seq, seq);
}

std::vector<rtp_extended_sequence_num_t> SequenceTracker::GetMissing()
{
    // List all missing packets not already NACK'd
    std::vector<rtp_extended_sequence_num_t> toNack;
    for (auto it = missing.begin(); it != missing.end(); ++it)
    {
        if (!nacksOutstanding.contains(*it))
        {
            toNack.emplace_back(*it);
        }
    }

    // If we have many outstandling NACKs, try to clean up the older ones that were likely lost
    if (toNack.size() + nacksOutstanding.size() > MAX_OUTSTANDING_NACKS)
    {
        auto now = std::chrono::steady_clock::now();
        for (auto it = nacksOutstanding.begin(); it != nacksOutstanding.end();)
        {
            if (now - it->second >= RECEIVE_BUFFER_TIMEOUT)
            {
                missing.erase(it->first);
                it = nacksOutstanding.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // Reverse so we hand back recent packets first
    std::reverse(toNack.begin(), toNack.end());

    // Finally, ensure we stay under the max outstanding nack limit
    if (toNack.size() + nacksOutstanding.size() > MAX_OUTSTANDING_NACKS)
    {
        spdlog::debug("Unable to NACK some missed packets; toNack: {}, nacksOutstanding: {}",
                      toNack.size(), nacksOutstanding.size());
        toNack.resize(MAX_OUTSTANDING_NACKS - nacksOutstanding.size());
    }

    return toNack;
}

uint64_t SequenceTracker::GetPacketsLost() const
{
    return packetsLost;
}

void SequenceTracker::resync()
{
    initialized = false;
    reorderBuffer.clear();
    receiveBuffer.clear();
    missing.clear();
    nacksOutstanding.clear();
    nackMapping.clear();
    maxSeq = 0;
    packetsSinceLastMissed = 0;
}

std::ostream &operator<<(std::ostream &os, const SequenceTracker &self)
{
    os << "SequenceTracker { "
       << "initialized:" << self.initialized << ", "
       << "received:" << self.packetsReceived << ", "
       << "missed:" << self.packetsMissed << ", "
       << "lost:" << self.packetsLost << ", "
       << "sinceLastMissed:" << self.packetsSinceLastMissed << ", "
       << "reorderBuffer.size:" << self.reorderBuffer.size() << ", "
       << "receiveBuffer.size:" << self.receiveBuffer.size() << ", "
       << "missing.size:" << self.missing.size() << ", "
       << "nacksOutstanding.size:" << self.nacksOutstanding.size() << ", "
       << "nackMapping.size:" << self.nackMapping.size() << " "
       << self.counter << " }";
    return os;
}

#pragma endregion Public methods

void SequenceTracker::missedPacket(rtp_extended_sequence_num_t seq)
{
    missing.emplace(seq);
    packetsMissed += 1;
    packetsLost += 1;
    packetsSinceLastMissed = 0;
}
