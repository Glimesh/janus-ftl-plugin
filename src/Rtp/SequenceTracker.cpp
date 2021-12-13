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
    auto extendResult = counter.Extend(seq);

    if (extendResult.reset)
    {
        spdlog::debug("Resetting extended sequence number for source");

        Reset();
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
    bool inserted = received.emplace(seq).second;

    if (!inserted)
    {
        // Duplicate packet received, nothing to do
        return false;
    }

    auto now = std::chrono::steady_clock::now();

    reorderBuffer.emplace(seq, now);

    // Cleanup reorder buffer
    for (auto it = reorderBuffer.begin(); it != reorderBuffer.end();)
    {
        if (reorderBuffer.size() > REORDER_BUFFER_SIZE || now - it->second >= REORDER_BUFFER_TIMEOUT)
        {
            auto seq = it->first;
            it = reorderBuffer.erase(it);
            checkForMissing(seq);
        }
        else
        {
            ++it;
        }
    }

    // Cleanup received buffer
    for (auto it = received.begin(); it != received.end() && received.size() > MAX_RECEIVE_BUFFER_SIZE;)
    {
        it = received.erase(it);
    }

    return true;
}

void SequenceTracker::checkForMissing(rtp_extended_sequence_num_t seq)
{
    if (!initialized)
    {
        watermark = seq;
        initialized = true;
    }

    missing.erase(seq);
    nacksOutstanding.erase(seq);

    int64_t gap = seq - watermark;
    if (gap == 1)
    {
        // In-order packet
        packetsSinceLastMissed += 1;
    }
    else if (gap > MAX_DROPOUT)
    {
        spdlog::warn("Missed {} packets, ignoring and waiting for stream to stabilize...",
                     gap);
    }
    else
    {
        // Mark all sequence numbers in gap as missing (if any)
        for (int64_t i = 1; i < gap; ++i)
        {
            missedPacket(watermark + i);
        }
    }

    if (seq > watermark)
    {
        watermark = seq;
    }

    for (auto it = missing.begin(); it != missing.end() && missing.size() > MAX_MISSING_SET_SIZE;)
    {
        nacksOutstanding.erase(*it);
        it = missing.erase(it);
    }

    auto now = std::chrono::steady_clock::now();
    for (auto it = nacksOutstanding.begin(); it != nacksOutstanding.end();)
    {
        if (now - it->second >= NACK_OUTSTANDING_TIMEOUT)
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

void SequenceTracker::NackSent(rtp_extended_sequence_num_t seq)
{
    nacksOutstanding.emplace(seq, std::chrono::steady_clock::now());

    for (auto it = nacksOutstanding.begin(); it != nacksOutstanding.end() && nacksOutstanding.size() > MAX_NACKS_OUTSTANDING_SET_SIZE;)
    {
        missing.erase(it->first);
        it = nacksOutstanding.erase(it);
    }
}

std::list<rtp_extended_sequence_num_t> SequenceTracker::GetMissing() const
{
    std::list<rtp_extended_sequence_num_t> toNack;

    // Iterate newest missing packets first
    for (auto it = missing.rbegin(); it != missing.rend() && nacksOutstanding.size() + toNack.size() < MAX_OUTSTANDING_NACKS; ++it)
    {
        auto seq = *it;
        if (!nacksOutstanding.contains(seq))
        {
            toNack.emplace_back(seq);
        }
    }

    if (nacksOutstanding.size() > MAX_OUTSTANDING_NACKS)
    {
        spdlog::debug("Unable to NACK some missed packets, too many outstanding NACKs: {}", nacksOutstanding.size());
    }

    return toNack;
}

void SequenceTracker::Reset()
{
    initialized = false;
    received.clear();
    missing.clear();
    nacksOutstanding.clear();
    watermark = 0;
    packetsSinceLastMissed = 0;
}

void SequenceTracker::missedPacket(rtp_extended_sequence_num_t seq)
{
    missing.emplace(seq);
    packetsMissed += 1;
    packetsSinceLastMissed = 0;
}

#pragma endregion Public methods
