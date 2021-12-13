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

    INFO("emplace - " << result);
    return true;
}

TEST_CASE("Sequence from zero with no missing packets")
{
    SequenceTracker counter;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(emplace(counter, seq));
        seq++;
    }
    REQUIRE(counter.GetMissing().size() == 0);
}

TEST_CASE("Sequence that wraps with no missing packets")
{
    SequenceTracker counter;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(emplace(counter, seq));
        seq++;
    }
    REQUIRE(counter.GetMissing().size() == 0);
}

TEST_CASE("Every other packet is missing")
{
    SequenceTracker counter;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 21 + SequenceTracker::REORDER_BUFFER_SIZE; ++i)
    {
        REQUIRE(emplace(counter, seq));
        seq += 2;
    }
    REQUIRE(counter.GetMissing().size() == 20);
}
