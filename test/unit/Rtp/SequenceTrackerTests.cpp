/**
 * @file SequenceTrackerTests.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-12-12
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Rtp/SequenceTracker.h"

using Catch::Matchers::Equals;

static const rtp_sequence_num_t MIN_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::min();
static const rtp_sequence_num_t MAX_SEQ_NUM = std::numeric_limits<rtp_sequence_num_t>::max();

bool emplace(
    SequenceTracker &tracker,
    rtp_extended_sequence_num_t seq,
    bool expectValid = true)
{
    auto extended = tracker.Track(seq);

    REQUIRE(extended == seq);
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
    for (int i = 0; i < 20 + SequenceTracker::REORDER_BUFFER_SIZE; ++i)
    {
        REQUIRE(emplace(tracker, seq));
        seq += 2;
    }
    REQUIRE(tracker.GetMissing().size() == 20);
}

TEST_CASE("Track two NACKs")
{
    SequenceTracker tracker;
    rtp_extended_sequence_num_t seq = MAX_SEQ_NUM - 100;

    // Run sequence for a bit
    for (int i = 0; i < 100; ++i)
    {
        emplace(tracker, seq);
        seq++;
    }

    INFO("Skipping two packet sequence numbers: " << seq << ", " << seq + 1);
    auto skipStart = seq++;
    seq++;

    // Send next few packets
    INFO("Send a few more packets ");
    for (int j = 0; j < 100; ++j)
    {
        emplace(tracker, seq);
        seq++;
    }

    REQUIRE(tracker.GetMissing().size() == 2);
    tracker.NackSent(skipStart);
    tracker.NackSent(skipStart + 1);
    REQUIRE(tracker.GetMissing().size() == 0);

    INFO("Receive skipped packets (simulating NACK) seq:" << seq);
    emplace(tracker, skipStart);
    emplace(tracker, skipStart + 1);

    REQUIRE(tracker.GetMissing().size() == 0);
}

TEST_CASE("Skip second packet")
{
    SequenceTracker tracker;
    std::vector<rtp_extended_sequence_num_t> skipped;
    rtp_extended_sequence_num_t seq = 0;

    INFO("Receive first packet");
    emplace(tracker, seq);
    seq++;

    INFO("Skip second packet");
    skipped.emplace_back(seq++);

    INFO("Flush reorder buffer");
    CAPTURE(seq);
    for (int i = 0; i < SequenceTracker::REORDER_BUFFER_SIZE + 50; ++i)
    {
        emplace(tracker, seq);
        seq++;
    }

    std::reverse(skipped.begin(), skipped.end());
    REQUIRE_THAT(tracker.GetMissing(), Equals(skipped));
}

TEST_CASE("Randomized skipped packet test")
{
    std::random_device rd;
    auto seed = rd();
    std::mt19937 gen(seed);
    std::uniform_int_distribution<rtp_sequence_num_t> seq_num_dis;
    std::uniform_real_distribution<> real_dis;

    SequenceTracker tracker;
    std::set<rtp_extended_sequence_num_t> skipped;
    rtp_extended_sequence_num_t seq = seq_num_dis(gen);

    float packetLoss = 0.05;

    CAPTURE(seed, seq);

    // Cannot skip first seq and get correct missing count
    emplace(tracker, seq);
    seq++;

    // Run sequence for a bit
    for (int i = 0; i < SequenceTracker::RECEIVE_BUFFER_SIZE; ++i)
    {
        if (real_dis(gen) < packetLoss && skipped.size() < SequenceTracker::MAX_OUTSTANDING_NACKS)
        {
            // Skip seq number to simulate lost packet
            skipped.emplace(seq);
        }
        else
        {
            emplace(tracker, seq);
        }
        seq++;
    }

    INFO("Flush reorder buffer");
    for (int i = 0; i < SequenceTracker::REORDER_BUFFER_SIZE; ++i)
    {
        emplace(tracker, seq);
        seq++;
    }

    REQUIRE(tracker.GetMissing().size() == skipped.size());
}

// TEST_CASE("Randomized skipped packet test (seed 1)")
// {
//     std::random_device rd;
//     auto seed = rd();
//     seed = 1818668368;
//     std::mt19937 gen(seed);
//     std::uniform_int_distribution<rtp_sequence_num_t> seq_num_dis;
//     std::uniform_real_distribution<> real_dis;

//     SequenceTracker tracker;
//     std::vector<rtp_extended_sequence_num_t> skipped;
//     rtp_extended_sequence_num_t seq = seq_num_dis(gen);

//     CAPTURE(seed, seq);

//     float packetLoss = 0.05;

//     // Cannot skip first seq and get correct missing count
//     emplace(tracker, seq);
//     seq++;

//     // Run sequence for a bit
//     for (int i = 0; i < SequenceTracker::RECEIVE_BUFFER_SIZE; ++i)
//     {
//         if (real_dis(gen) < packetLoss && skipped.size() < SequenceTracker::MAX_OUTSTANDING_NACKS)
//         {
//             // Skip seq number to simulate lost packet
//             WARN("Skipping " << seq);
//             skipped.emplace_back(seq);
//         }
//         else
//         {
//             emplace(tracker, seq);
//         }
//         seq++;
//     }

//     INFO("Flush reorder buffer");
//     CAPTURE(seq);
//     for (int i = 0; i < SequenceTracker::REORDER_BUFFER_SIZE; ++i)
//     {
//         emplace(tracker, seq);
//         seq++;
//     }

//     std::reverse(skipped.begin(), skipped.end());
//     REQUIRE_THAT(tracker.GetMissing(), Equals(skipped));
// }

TEST_CASE("Randomized skip and re-transmit packet test")
{
    std::random_device rd;
    auto seed = rd();
    seed = 2032656355;
    std::mt19937 gen(seed);
    std::uniform_int_distribution<rtp_sequence_num_t> seq_num_dis;
    std::uniform_real_distribution<> real_dis;

    SequenceTracker tracker;
    std::set<rtp_extended_sequence_num_t> skipped;
    rtp_extended_sequence_num_t seq = seq_num_dis(gen);

    float packetLoss = 0.01;

    CAPTURE(seed, seq);

    // Cannot skip first seq and get correct missing count
    emplace(tracker, seq);
    seq++;

    // Run sequence for a bit
    for (int i = 0; i < 100000; ++i)
    {
        CAPTURE(seq);

        if (real_dis(gen) < packetLoss && skipped.size() < SequenceTracker::MAX_OUTSTANDING_NACKS)
        {
            INFO("Skiping seq:" << seq << " to simulate a lost packet");
            skipped.emplace(seq);
        }
        else
        {
            emplace(tracker, seq);
        }
        seq++;

        if (i % 1000 == 0)
        {
            // Re-transmit pending NACK's every hundred iterations to simulate network delay
            auto missing = tracker.GetMissing();
            for (auto it = missing.begin(); it != missing.end(); ++it)
            {
                INFO("Re-transmitting seq:" << *it);
                tracker.NackSent(*it);
                skipped.erase(*it);
                emplace(tracker, *it);
            }
        }
    }

    INFO("Flush reorder buffer");
    for (int i = 0; i < SequenceTracker::REORDER_BUFFER_SIZE; ++i)
    {
        emplace(tracker, seq);
        seq++;
    }

    REQUIRE(tracker.GetMissing().size() == skipped.size());
}
