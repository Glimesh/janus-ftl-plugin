/**
 * @file RtpPacket.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-30
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include "RtpPacket.h"

#include <netinet/in.h>

#pragma region Utility methods
const RtpHeader* RtpPacket::GetRtpHeader(const std::vector<std::byte>& rtpPacket)
{
    return reinterpret_cast<const RtpHeader*>(rtpPacket.data());
}

const rtp_sequence_num_t RtpPacket::GetRtpSequence(const std::vector<std::byte>& rtpPacket)
{
    return ntohs(GetRtpHeader(rtpPacket)->SequenceNumber);
}

const std::span<const std::byte> RtpPacket::GetRtpPayload(const std::vector<std::byte>& rtpPacket)
{
    if (rtpPacket.size() < 12)
    {
        return std::span<std::byte>();
    }

    const RtpHeader* rtpHeader = GetRtpHeader(rtpPacket);
    if (rtpHeader->Version != 2)
    {
        return std::span<std::byte>();
    }

    // RTP header is 12 bytes
    size_t payloadIndex = 12;
    
    // 4 bytes for every Csrc
    if (rtpHeader->CsrcCount > 0)
    {
        payloadIndex += (rtpHeader->CsrcCount * 4);
    }

    // Account for size of RTP header extensions
    if ((rtpHeader->Extension > 0) && (rtpPacket.size() >= (payloadIndex + 4)))
    {
        const RtpHeaderExtension* extension = 
            reinterpret_cast<const RtpHeaderExtension*>(rtpPacket.data() + payloadIndex);
        // Extension header is 4 bytes, extension length is in 32-bit words
        payloadIndex += 4 + (ntohs(extension->Length) * 4);
    }

    // Check for invalid size
    if ((rtpPacket.size() - payloadIndex) <= 0)
    {
        return std::span<std::byte>();
    }

    return std::span(rtpPacket.begin() + payloadIndex, rtpPacket.end());
}
#pragma endregion Utility methods