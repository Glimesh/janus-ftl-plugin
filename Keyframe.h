/**
 * @file Keyframe.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-09-11
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <stdint.h>
#include <list>
#include <memory>
#include <vector>

struct Keyframe
{
    Keyframe() : rtpTimestamp(0) { }
    uint32_t rtpTimestamp;
    std::list<std::shared_ptr<std::vector<unsigned char>>> rtpPackets;
};