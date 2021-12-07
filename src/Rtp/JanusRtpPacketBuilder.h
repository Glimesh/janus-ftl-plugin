/**
 * @file JanusRtpPacketBuilder.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-12-06
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#pragma once

#include "Types.h"

#include <assert.h>
#include <span>

extern "C"
{
    #include <plugins/plugin.h>
}

class JanusRtpPacketBuilder
{
private:
    /* Private fields */
    std::vector<std::byte> buffer;
    janus_plugin_rtp_extensions extensions;

public:
    /* Constructor/Destructor */
    JanusRtpPacketBuilder(std::span<const std::byte> packet) : buffer(packet.begin(), packet.end())
    {
        janus_plugin_rtp_extensions_reset(&this->extensions);
    }

    /* Public methods */
    janus_plugin_rtp Build(rtp_payload_type_t videoPayloadType);

#if defined(JANUS_PLAYOUT_DELAY_SUPPORT)
    JanusRtpPacketBuilder &PlayoutDelay(int16_t min, int16_t max)
    {
        assert(min > 0);
        assert(max > 0);
        assert(max >= min);
        assert(min <= 0x0FFF);
        assert(max <= 0x0FFF);
        this->extensions.playout_delay_min = min;
        this->extensions.playout_delay_max = max;
        return *this;
    }
#endif
};
