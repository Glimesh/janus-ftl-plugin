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
    void GenerateImage(const Keyframe& keyframe) override;
};