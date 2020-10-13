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

    virtual void GenerateImage(const Keyframe& keyframe) = 0;
};