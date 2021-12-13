/**
 * @file SequenceTrackerTests.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-12-12
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Rtp/SequenceTracker.h"

static const rtp_sequence_num_t MAX_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::max();

bool emplace(
    SequenceTracker& tracker,
    rtp_extended_sequence_num_t seq,
    bool expectValid = true)
{
    auto result = tracker.Track(seq);

    REQUIRE(result == seq);
    return true;
}

TEST_CASE("Sequence from zero with no missing packets")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(emplace(tracker, seq));
        seq++;
    }
    REQUIRE(tracker.GetMissing().size() == 0);
}

TEST_CASE("Sequence that wraps with no missing packets")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(emplace(tracker, seq));
        seq++;
    }
    REQUIRE(tracker.GetMissing().size() == 0);
}

TEST_CASE("Every other packet is missing")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 21 + SequenceTracker::REORDER_BUFFER_SIZE; ++i)
    {
        REQUIRE(emplace(tracker, seq));
        seq += 2;
    }
    REQUIRE(tracker.GetMissing().size() == 20);
}

// TODO, breaks because packet gets wrong seq number
// After extended counter wraps, the retransmitted packets skip ahead from expected
// (MAX_SEQ_NUM - 1) to next cycle (2*MAX_SEQ_NUM - 1)
TEST_CASE("Track two NACKs")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t extended = MAX_SEQ_NUM - 100;

    // Run sequence for a bit
    for (int i = 0; i < 100; ++i)
    {
        emplace(tracker, extended);
        extended++;
    }

    INFO("Skipping two packet sequence numbers: " << extended << ", " << extended + 1);
    auto skipStart = extended++;
    extended++;

    // Send next few packets
    INFO("Send a few more packets ");
    for (int j = 0; j < 100; ++j)
    {
        emplace(tracker, extended);
        extended++;
    }
    
    REQUIRE(tracker.GetMissing().size() == 2);
    tracker.NackSent(skipStart);
    tracker.NackSent(skipStart + 1);
    REQUIRE(tracker.GetMissing().size() == 0);

    INFO("Receive skipped packets (simulating NACK) extended:" << extended);
    emplace(tracker, skipStart);
    emplace(tracker, skipStart + 1);

    REQUIRE(tracker.GetMissing().size() == 0);
}
