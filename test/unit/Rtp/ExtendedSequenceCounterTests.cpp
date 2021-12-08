/**
 * @file ExtendedSequenceCounterTest.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Rtp/ExtendedSequenceCounter.h"

static const rtp_sequence_num_t MAX_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::max();

void extend(
    ExtendedSequenceCounter& counter,
    rtp_sequence_num_t seq,
    rtp_extended_sequence_num_t expectedExtendedSeq,
    bool expectValid = true)
{
    auto result = counter.Extend(seq);

    INFO("extend - seq:" << seq << " extended:" << result.extendedSeq << " valid:" << result.valid);
    INFO("" << counter);
    REQUIRE(result.extendedSeq == expectedExtendedSeq);
    CHECK(result.valid == expectValid);
}

TEST_CASE("Sequence from zero is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t extended = 0;
    for (int i = 0; i < 100; ++i)
    {
        extend(counter, extended, extended);
        extended++;
    }
}

TEST_CASE("Sequence that wraps is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t extended = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 100; ++i)
    {
        extend(counter, extended, extended);
        extended++;
    }
}

TEST_CASE("A skip less than MAX_DROPOUT is treated as valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t extended = 0;
    for (int i = 0; i <= ExtendedSequenceCounter::MIN_SEQUENTIAL; ++i)
    {
        extend(counter, extended, extended);
        extended++;
    }

    // Skip ahead less than MAX_DROPOUT
    extended += ExtendedSequenceCounter::MAX_DROPOUT - 1;

    for (int i = 0; i < 10; ++i)
    {
        extend(counter, extended, extended);
        extended++;
    }
}

TEST_CASE("A small skip accross a wrap is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t extended = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 10; ++i, extended++)
    {
        extend(counter, extended, extended);
    }

    // Skip ahead less than MIN_DROPOUT but enough to wrap-around
    extended += 100;

    for (int i = 0; i < 10; ++i, ++extended)
    {
        extend(counter, extended, extended);
    }
}
