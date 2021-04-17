/**
 * @file ExtendedSequenceCounterTest.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Rtp/ExtendedSequenceCounter.h"

const rtp_sequence_num_t MAX_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::max();

void extend(
    ExtendedSequenceCounter& counter,
    rtp_sequence_num_t seq,
    rtp_extended_sequence_num_t expectedExtendedSeq,
    bool expectValid = true)
{
    rtp_extended_sequence_num_t extendedSeq;
    bool valid = counter.Extend(seq, &extendedSeq);

    INFO("extend - seq:" << seq << " extended:" << extendedSeq << " valid:" << valid);
    INFO("" << counter);
    REQUIRE(extendedSeq == expectedExtendedSeq);
    CHECK(valid == expectValid);
}

TEST_CASE("Sequence from zero is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t extended = 0;
    for (int i = 0; i < 100; ++i, ++extended)
    {
        extend(counter, extended, extended);
    }
}

TEST_CASE("Sequence that wraps is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t extended = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 100; ++i, ++extended)
    {
        extend(counter, extended, extended);
    }
}
