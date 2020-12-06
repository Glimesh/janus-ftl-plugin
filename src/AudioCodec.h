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

enum class AudioCodecKind
{
    Unsupported = 0,
    Opus
};

class SupportedAudioCodecs
{
public:
    static AudioCodecKind ParseAudioCodec(const std::string codec)
    {
        std::string lowercaseCodec = codec;
        std::transform(
            lowercaseCodec.begin(),
            lowercaseCodec.end(),
            lowercaseCodec.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (lowercaseCodec.compare("opus") == 0)
        {
            return AudioCodecKind::Opus;
        }
        return AudioCodecKind::Unsupported;
    }

    static std::string AudioCodecString(const AudioCodecKind codec)
    {
        switch (codec)
        {
        case AudioCodecKind::Opus:
            return "opus";
        case AudioCodecKind::Unsupported:
        default:
            return "";
        }
    }
};