/**
 * @file Rtp.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-30
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "Types.h"

#include <cstdint>
#include <span>
#include <vector>

/**
 * @brief RTP Class providing a bunch of RTP packet related utilities!
 */
class RtpPacket
{
public:

    /* Utility methods */
    static const RtpHeader* GetRtpHeader(const std::vector<std::byte>& rtpPacket);
    static const rtp_sequence_num_t GetRtpSequence(const std::vector<std::byte>& rtpPacket);
    static const std::span<const std::byte> GetRtpPayload(const std::vector<std::byte>& rtpPacket);
};