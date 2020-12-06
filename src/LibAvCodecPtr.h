/**
 * @file LibAvCodecPtr.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @brief RAII smart pointer wrappers for libavcodec pointers
 * @version 0.1
 * @date 2020-10-16
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */
#pragma once

#include <memory>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

// RAII Pointer for AVCodecContext
struct AVCodecContextUnref
{
    void operator() (AVCodecContext* context)
    {
        if (context != nullptr)
        {
            avcodec_free_context(&context);
        }
    }
};
typedef
    std::unique_ptr<
        AVCodecContext,
        AVCodecContextUnref> AVCodecContextPtr;

// RAII Pointer for AVFrame
struct AVFrameUnref
{
    void operator() (AVFrame* frame)
    {
        if (frame != nullptr)
        {
            av_frame_free(&frame);
        }
    }
};
typedef
    std::unique_ptr<
        AVFrame,
        AVFrameUnref> AVFramePtr;

// RAII Pointer for AVPacket
struct AVPacketUnref
{
    void operator() (AVPacket* packet)
    {
        if (packet != nullptr)
        {
            av_packet_free(&packet);
        }
    }
};
typedef
    std::unique_ptr<
        AVPacket,
        AVPacketUnref> AVPacketPtr;