#pragma once

#include "Types.h"

#include <cstdint>

typedef uint64_t rtp_extended_sequence_num_t;

/**
 * @brief
 * 
 * @see https://www.rfc-editor.org/rfc/rfc3550.html#appendix-A.1
 */
class ExtendedSequenceCounter
{
public:
    bool Extend(rtp_sequence_num_t seq, rtp_extended_sequence_num_t* extendedSeq);

    friend std::ostream& operator<<(std::ostream & out, const ExtendedSequenceCounter& point);
private:
    const int MAX_DROPOUT = 3000;
    const int MAX_MISORDER = 100;
    const int MIN_SEQUENTIAL = 2;

    rtp_sequence_num_t maxSeq;
    uint32_t cycles = 0;
    uint32_t baseSeq;
    uint32_t badSeq = RTP_SEQ_MOD + 1;
    uint32_t probation = MIN_SEQUENTIAL;
    uint32_t received = 0;
    uint32_t receivedPrior = 0;
    uint32_t expectedPrior = 0;
    bool initialized = false;

    bool UpdateState(rtp_sequence_num_t seq);
    void Initialize(rtp_sequence_num_t seq);
    void Reset(rtp_sequence_num_t seq);
};
