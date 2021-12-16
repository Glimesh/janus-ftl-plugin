/**
 * @file ExtendedSequenceCounter.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include "ExtendedSequenceCounter.h"

#include <iostream>

bool ExtendedSequenceCounter::Extend(rtp_sequence_num_t seq, rtp_extended_sequence_num_t* extendedSeq)
{
    bool valid = UpdateState(seq);
    *extendedSeq = cycles | seq;
    return valid;
}

bool ExtendedSequenceCounter::UpdateState(rtp_sequence_num_t seq)
{
    if (!initialized)
    {
        Initialize(seq);
        return true;
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
                Reset(seq);
                received++;
                return true;
            }
        }
        else
        {
            probation = MIN_SEQUENTIAL - 1;
            maxSeq = seq;
        }
        /*
        * Unlike RFC 3550, we consider a packet valid even if less than MIN_SEQUENTIAL
        * have been received as long it is sequential to all packets received so far.
        */ 
        return true;
    } else if (udelta < MAX_DROPOUT) {
        /* in order, with permissible gap */
        if (seq < maxSeq) {
            /*
            * Sequence number wrapped - count another 64K cycle.
            */
            cycles += RTP_SEQ_MOD;
        }
        maxSeq = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
        /* the sequence number made a very large jump */
        if (seq == badSeq) {
            /*
            * Two sequential packets -- assume that the other side
            * restarted without telling us so just re-sync
            * (i.e., pretend this was the first packet).
            */
            Reset(seq);
        }
        else {
            badSeq = (seq + 1) & (RTP_SEQ_MOD-1);
            return false;
        }
    } else {
        /* duplicate or reordered packet */
    }
    received++;
    return true;
}

void ExtendedSequenceCounter::Initialize(rtp_sequence_num_t seq)
{
    Reset(seq);
    baseSeq = seq;
    maxSeq = seq - 1;
    initialized = true;
}

void ExtendedSequenceCounter::Reset(rtp_sequence_num_t seq)
{
    baseSeq = seq;
    maxSeq = seq;
    badSeq = RTP_SEQ_MOD + 1;   /* so seq == badSeq is false */
    cycles = 0;
    received = 0;
    receivedPrior = 0;
    expectedPrior = 0;
}

std::ostream& operator<<(std::ostream & os, const ExtendedSequenceCounter& c)
{
    os << "ExtendedSequenceCounter { " <<
        "maxSeq:"   << c.maxSeq   << ", " <<
        "cycles:"   << c.cycles   << ", " <<
        "baseSeq:"  << c.baseSeq  << ", " <<
        "received:" << c.received <<
        " }";
    return os;
}
