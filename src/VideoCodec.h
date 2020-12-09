/**
 * @file VideoCodec.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-08-24
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once
#include <algorithm>
#include <string>

enum class VideoCodecKind
{
    Unsupported = 0,
    H264
};

class SupportedVideoCodecs
{
public:
    static VideoCodecKind ParseVideoCodec(const std::string codec)
    {
        std::string lowercaseCodec = codec;
        std::transform(
            lowercaseCodec.begin(),
            lowercaseCodec.end(),
            lowercaseCodec.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (lowercaseCodec.compare("h264") == 0)
        {
            return VideoCodecKind::H264;
        }
        return VideoCodecKind::Unsupported;
    }

    static std::string VideoCodecString(const VideoCodecKind codec)
    {
        switch (codec)
        {
        case VideoCodecKind::H264:
            return "H264";
        case VideoCodecKind::Unsupported:
        default:
            return "";
        }
    }
};