/**
 * @file ExtendedSequenceCounter.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#pragma once

#include "Types.h"

#include <cstdint>

typedef uint64_t rtp_extended_sequence_num_t;

/**
 * @brief Stateful counter to handle wrapping of two-byte RTP sequence numbers by extending them.
 * 
 * @see https://www.rfc-editor.org/rfc/rfc3550.html#appendix-A.1
 */
class ExtendedSequenceCounter
{
public:
    struct ExtendResult {
        rtp_extended_sequence_num_t extendedSeq;
        bool valid;
        bool reset;
    };

    ExtendResult Extend(rtp_sequence_num_t seq);

    friend std::ostream& operator<<(std::ostream & out, const ExtendedSequenceCounter& self);
    
    static const rtp_sequence_num_t MAX_DROPOUT = 3000;
    static const rtp_sequence_num_t MAX_MISORDER = 100;
    static const rtp_sequence_num_t MIN_SEQUENTIAL = 2;
private:
    rtp_sequence_num_t maxSeq = 0;
    uint32_t cycles = 0;
    uint32_t baseSeq = 0;
    uint32_t badSeq = RTP_SEQ_MOD + 1;
    uint32_t probation = MIN_SEQUENTIAL;
    uint32_t received = 0;
    uint32_t receivedPrior = 0;
    uint32_t expectedPrior = 0;
    bool initialized = false;

    void Initialize(rtp_sequence_num_t seq);
    void Reset(rtp_sequence_num_t seq);
};
