/**
 * @file RtpPacket.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-30
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Types.h"
#include "ExtendedSequenceCounter.h"

/**
 * @brief RTP Class providing a bunch of RTP packet related utilities!
 */
class RtpPacket
{
public:
    /* Static utility methods */
    static const RtpHeader* GetRtpHeader(std::span<const std::byte> rtpPacket);
    static const rtp_sequence_num_t GetRtpSequence(std::span<const std::byte> rtpPacket);
    static const std::span<const std::byte> GetRtpPayload(std::span<const std::byte> rtpPacket);

    /* Constructor/Destructor */
    RtpPacket(
        std::span<const std::byte> bytes,
        rtp_extended_sequence_num_t extendedSequenceNum);

    /* Public fields */
    const std::vector<std::byte> Bytes;
    const rtp_extended_sequence_num_t ExtendedSequenceNum;

    /* Public methods */
    const RtpHeader* Header() const;
    const rtp_sequence_num_t SequenceNum() const;
    const std::span<const std::byte> Payload() const;
};