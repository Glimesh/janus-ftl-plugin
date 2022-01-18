/**
 * @file SequenceTrackerTests.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-12-12
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Rtp/SequenceTracker.h"

#include <thread>

using namespace std::literals;
using Catch::Matchers::Equals;

static const rtp_sequence_num_t MIN_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::min();
static const rtp_sequence_num_t MAX_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::max();

bool track(
    SequenceTracker &tracker,
    rtp_extended_sequence_num_t seq,
    bool expectValid = true)
{
    CAPTURE(seq, tracker);
    REQUIRE(tracker.Track(seq) == seq);
    return true;
}

size_t flushReorderBuffer(
    SequenceTracker &tracker,
    rtp_extended_sequence_num_t &seq)
{
    INFO("Flush reorder buffer");
    for (int i = 0; i <= SequenceTracker::REORDER_DELTA; ++i)
    {
        track(tracker, seq);
        seq++;
    }
    return SequenceTracker::REORDER_DELTA + 1;
}

TEST_CASE("Sequence from zero with no missing packets")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(track(tracker, seq));
        seq++;
    }
    CAPTURE(seq, tracker);
    CHECK(tracker.GetNackList().size() == 0);
    CHECK(tracker.GetReceivedCount() == 100);
    CHECK(tracker.GetMissedCount() == 0);
}

TEST_CASE("Sequence that wraps with no missing packets")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 50;
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(track(tracker, seq));
        seq++;
    }
    CAPTURE(seq, tracker);
    CHECK(tracker.GetNackList().size() == 0);
    CHECK(tracker.GetReceivedCount() == 100);
    CHECK(tracker.GetMissedCount() == 0);
}

TEST_CASE("Every other packet is missing")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = 0;
    for (int i = 0; i < 20; ++i)
    {
        REQUIRE(track(tracker, seq));
        seq += 2;
    }

    auto flushedPackets = flushReorderBuffer(tracker, seq);

    CAPTURE(seq, tracker);
    CHECK(tracker.GetNackList().size() == 20);
    CHECK(tracker.GetReceivedCount() == 20 + flushedPackets);
    CHECK(tracker.GetMissedCount() == 20);
}

TEST_CASE("Two consecutive skipped packets, nack, then retransmit")
{
    SequenceTracker tracker;
    std::vector<rtp_extended_sequence_num_t> skipped;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 100;

    INFO("Send a few packets");
    for (int i = 0; i < 100; ++i)
    {
        track(tracker, seq);
        seq++;
    }

    INFO("Skip two packet sequence numbers: " << seq << ", " << seq + 1);
    CAPTURE(seq, tracker);
    skipped.emplace_back(seq++);
    skipped.emplace_back(seq++);

    // Reverse skipped list, later methods like GetNackList sort higher (most recent) numbers first
    std::reverse(skipped.begin(), skipped.end());

    INFO("Send a few more packets");
    for (int j = 0; j < 100; ++j)
    {
        track(tracker, seq);
        seq++;
    }

    REQUIRE_THAT(tracker.GetNackList(), Equals(skipped));
    CHECK(tracker.GetMissedCount() == 2);

    INFO("Simulate sending NACKs");
    for (auto s : skipped) {
        tracker.MarkNackSent(s);
    }

    CHECK(tracker.GetNackList().size() == 0);
    CHECK(tracker.GetMissedCount() == 2);

    INFO("Receive skipped packets to simulate re-transmits due to the NACKs");
    CAPTURE(seq, tracker);
    for (auto s : skipped) {
        INFO("track");
        track(tracker, s);
    }

    CHECK(tracker.GetNackList().size() == 0);
    CHECK(tracker.GetLostCount() == 0);
}

TEST_CASE("Skip second packet")
{
    SequenceTracker tracker;
    std::vector<rtp_extended_sequence_num_t> skipped;
    rtp_extended_sequence_num_t seq = 0;

    INFO("Receive first packet");
    track(tracker, seq);
    seq++;

    INFO("Skip second packet");
    skipped.emplace_back(seq++);

    flushReorderBuffer(tracker, seq);

    std::reverse(skipped.begin(), skipped.end());
    REQUIRE_THAT(tracker.GetNackList(), Equals(skipped));
}

TEST_CASE("Many outstanding NACKs")
{
    SequenceTracker tracker;
    std::vector<rtp_extended_sequence_num_t> skipped;
    rtp_extended_sequence_num_t seq = 0;

    INFO("Receive a number of packets with a high loss rate");
    CAPTURE(seq);
    for (int i = 0; i < 1000; ++i)
    {
        // Receive one packet
        track(tracker, seq);
        seq++;

        // Skip next packet
        seq++;
    }

    flushReorderBuffer(tracker, seq);

    INFO("Send NACKs, but don't retransmit packet to simulate many outstanding NACKs");
    for (auto missing : tracker.GetNackList())
    {
        tracker.MarkNackSent(missing);
    }

    INFO("Receive another bunch of packets with a high loss rate");
    CAPTURE(seq);
    for (int i = 0; i < 1000; ++i)
    {
        // Receive one packet
        track(tracker, seq);
        seq++;

        // Skip next packet and record it
        skipped.emplace_back(seq++);
    }

    flushReorderBuffer(tracker, seq);

    INFO("Wait for outstanding NACKs to timeout");
    std::this_thread::sleep_for(2s);

    std::reverse(skipped.begin(), skipped.end());
    std::vector<rtp_extended_sequence_num_t> expected(skipped.begin(), skipped.begin() + SequenceTracker::MAX_OUTSTANDING_NACKS);
    REQUIRE_THAT(tracker.GetNackList(), Equals(expected));
}
