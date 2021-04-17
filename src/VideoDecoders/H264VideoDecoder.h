/**
 * @file H264VideoDecoder.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-10-13
 *
 * @copyright Copyright (c) 2020 Hayden McAfee
 *
 */

#pragma once

#include "VideoDecoder.h"
#include "../Utilities/FtlTypes.h"
#include "../Utilities/LibAvCodecPtr.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
}

/**
 * @brief
 *  H264VideoDecoder is the VideoDecoder implementation for streams utilizing
 *  H264 video encoding.
 */
class H264VideoDecoder :
    public VideoDecoder
{
public:
    /* VideoDecoder */
    std::pair<uint16_t, uint16_t> ReadVideoDimensions(
        const std::list<std::vector<std::byte>>& keyframePackets) override;
    std::vector<uint8_t> GenerateJpegImage(
        const std::list<std::vector<std::byte>>& keyframePackets) override;

private:
    AVFramePtr readFramePtr(const std::list<std::vector<std::byte>>& keyframePackets);
    std::vector<uint8_t> encodeToJpeg(AVFramePtr frame);
};
