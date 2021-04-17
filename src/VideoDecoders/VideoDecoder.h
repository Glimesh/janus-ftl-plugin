/**
 * @file VideoDecoder.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-10-13
 *
 * @copyright Copyright (c) 2020 Hayden McAfee
 *
 */

#pragma once

#include <cstdint>
#include <list>
#include <vector>

struct Keyframe;

/**
 * @brief
 *  VideoDecoder is a generic interface to decode video streams for various use cases
 *  eg: generating thumbnails, reading video dimensions, etc.
 */
class VideoDecoder
{
public:
    virtual ~VideoDecoder()
    { }

    virtual std::pair<uint16_t, uint16_t> ReadVideoDimensions(
        const std::list<std::vector<std::byte>>& keyframePackets) = 0;

    virtual std::vector<uint8_t> GenerateJpegImage(
        const std::list<std::vector<std::byte>>& keyframePackets) = 0;
};
