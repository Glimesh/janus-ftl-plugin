/**
 * @file ExtendedSequenceCounter.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include "ExtendedSequenceCounter.h"

#include <iostream>
#include <spdlog/spdlog.h>

ExtendedSequenceCounter::ExtendResult ExtendedSequenceCounter::Extend(rtp_sequence_num_t seq)
{
    if (!initialized)
    {
        reset(seq);
        initialized = true;
        /*
         * Unlike RFC 3550, we consider a packet valid even if less than MIN_SEQUENTIAL
         * have been received as long it is sequential to all packets received so far.
         */
        return ExtendResult{
            .extendedSeq = cycles | seq,
            .valid = true,
            .resync = false,
            .stable = false,
        };
    }

    uint16_t udelta = seq - maxSeq;

    /*
     * Source is not considered stable until MIN_SEQUENTIAL packets with
     * sequential sequence numbers have been received.
     */
    if (probation > 0)
    {
        /* packet is in sequence */
        if (seq == maxSeq + 1)
        {
            probation--;
            maxSeq = seq;
            if (probation == 0)
            {
                reset(seq);
                return ExtendResult{
                    .extendedSeq = cycles | seq,
                    .valid = true,
                    .resync = false,
                    .stable = false,
                };
            }
        }
        else
        {
            probation = MIN_SEQUENTIAL - 1;
            maxSeq = seq;
        }
        return ExtendResult{
            .extendedSeq = cycles | seq,
            .valid = true,
            .resync = false,
            .stable = true,
        };
    }
    /* 
     * Allow small jumps due to lost packets. We allow up to and including for simpler
     * reasoning, the RFC 3550 example is slightly different and does (udelta < MAX_DROPOUT)
     */
    else if (udelta <= MAX_DROPOUT)
    {
        /* in order, with permissible gap */
        if (seq < maxSeq)
        {
            /*
             * Sequence number wrapped - count another 64K cycle.
             */
            cycles += RTP_SEQ_MOD;
        }
        maxSeq = seq;
        return ExtendResult{
            .extendedSeq = cycles | seq,
            .valid = true,
            .resync = false,
            .stable = true,
        };
    }
    else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER)
    {
        /* the sequence number made a very large jump */
        if (seq == badSeq)
        {
            /*
             * Two sequential packets -- assume that the other side
             * restarted without telling us so just re-sync
             * (i.e., pretend this was the first packet).
             */
            spdlog::trace("Sequence counter reset");
            reset(seq);
            return ExtendResult{
                .extendedSeq = cycles | seq,
                .valid = false,
                .resync = true,
                .stable = false,
            };
        }
        else
        {
            badSeq = (seq + 1) & (RTP_SEQ_MOD - 1);
            return ExtendResult{
                .extendedSeq = cycles | seq,
                .valid = false,
                .resync = false,
                .stable = true,
            };
        }
    }
    else
    {
        /* duplicate or reordered packet */
        return ExtendResult{
            .extendedSeq = cycles | seq,
            .valid = true,
            .resync = false,
            .stable = true,
        };
    }
}

/**
 * Per RFC 3550:
 * When a new source is heard for the first time, that is, its SSRC
 * identifier is not in the table (see Section 8.2), and the per-source
 * state is allocated for it, s->probation is set to the number of
 * sequential packets required before declaring a source valid
 * (parameter MIN_SEQUENTIAL) and other variables are initialized:
 * 
 *    init_seq(s, seq);
 *    s->max_seq = seq - 1;
 *    s->probation = MIN_SEQUENTIAL;
 */
void ExtendedSequenceCounter::initialize(rtp_sequence_num_t seq)
{
    reset(seq);
    baseSeq = seq;
    maxSeq = seq - 1;
    initialized = true;
}

void ExtendedSequenceCounter::reset(rtp_sequence_num_t seq)
{
    baseSeq = seq;
    maxSeq = seq;
    badSeq = RTP_SEQ_MOD + 1; /* so seq == badSeq is false */
    cycles = 0;
    receivedPrior = 0;
    expectedPrior = 0;
}

std::ostream &operator<<(std::ostream &os, const ExtendedSequenceCounter &self)
{
    os << "ExtendedSequenceCounter { "
       << "maxSeq:" << self.maxSeq << ", "
       << "cycleCount:" << self.cycles / RTP_SEQ_MOD << ", "
       << "baseSeq:" << self.baseSeq << " }";
    return os;
}
