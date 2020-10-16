/**
 * @file H264PreviewGenerator.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-10-13
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include "PreviewGenerator.h"
#include "LibAvCodecPtr.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
}

/**
 * @brief
 *  H264PreviewGenerator is the PreviewGenerator implementation for streams utilizing
 *  H264 video encoding.
 */
class H264PreviewGenerator :
    public PreviewGenerator
{
public:
    /* PreviewGenerator */
    std::vector<uint8_t> GenerateJpegImage(const Keyframe& keyframe) override;

private:
    std::vector<uint8_t> encodeToJpeg(AVFramePtr frame);
};