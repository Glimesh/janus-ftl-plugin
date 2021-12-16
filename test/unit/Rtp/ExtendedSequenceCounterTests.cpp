/**
 * @file ExtendedSequenceCounterTest.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Rtp/ExtendedSequenceCounter.h"

#include <string>
#include <sstream>

using Catch::Matchers::Equals;

static const rtp_sequence_num_t MAX_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::max();

template <typename T>
std::string to_string( const T& value )
{
  std::ostringstream ss;
  ss << value;
  return ss.str();
}

void extend(
    ExtendedSequenceCounter& counter,
    rtp_sequence_num_t seq,
    rtp_extended_sequence_num_t expected,
    bool expectValid = true)
{
    auto result = counter.Extend(seq);
    rtp_extended_sequence_num_t extended = result.extendedSeq;

    CAPTURE(seq, extended, expected, result.valid, counter);
    REQUIRE(extended == expected);
    CHECK(result.valid == expectValid);
}

TEST_CASE("Sequence from zero is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 100; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }
}

TEST_CASE("Sequence that wraps is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 100; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }
}

TEST_CASE("A skip less than MAX_DROPOUT is treated as valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t seq = 0;

    INFO("Send MIN_SEQUENTIAL packets to initialize counter");
    for (int i = 0; i <= ExtendedSequenceCounter::MIN_SEQUENTIAL; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }

    INFO("Skip less than MAX_DROPOUT packets");
    CAPTURE(seq, counter);
    seq += ExtendedSequenceCounter::MAX_DROPOUT - 1;

    INFO("Send a few more packets");
    for (int i = 0; i < 10; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }
}

TEST_CASE("A small skip accross a wrap is valid")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 50;

    INFO("Send a few packets");
    for (int i = 0; i < 10; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }

    INFO("Skip ahead less than MIN_DROPOUT but enough to wrap-around");
    CAPTURE(seq, counter);
    seq += 100;

    for (int i = 0; i < 10; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }
}

TEST_CASE("NACKs should not reset sequence counter")
{
    ExtendedSequenceCounter counter;
    rtp_extended_sequence_num_t seq = 0;

    INFO("Send a few packets");
    for (int i = 0; i < 100; ++i)
    {
        extend(counter, seq, seq);
        seq++;
    }

    INFO("Skip two packet sequence numbers: " << seq << ", " << seq + 1);
    CAPTURE(seq, counter);
    auto skipped = seq++;
    seq++;

    INFO("Send a few more packets");
    CAPTURE(seq, counter);
    for (int j = 0; j < 10; ++j)
    {
        extend(counter, seq, seq);
        seq++;
    }

    INFO("Receive skipped packets (simulating NACK re-transmits)");
    CAPTURE(seq, counter);
    extend(counter, skipped, skipped);
    extend(counter, skipped + 1, skipped + 1);
}
