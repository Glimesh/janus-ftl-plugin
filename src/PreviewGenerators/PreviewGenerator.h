/**
 * @file PreviewGenerator.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-10-13
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <cstdint>
#include <vector>

struct Keyframe;

/**
 * @brief 
 *  PreviewGenerator is a generic interface to generate previews from video streams
 *  (still images, thumbnails, motion previews eventually...)
 */
class PreviewGenerator
{
public:
    virtual ~PreviewGenerator()
    { }

    virtual std::vector<uint8_t> GenerateJpegImage(
        const std::list<std::vector<std::byte>>& keyframePackets) = 0;
};