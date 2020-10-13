/**
 * @file H264PreviewGenerator.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @version 0.1
 * @date 2020-10-13
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#include "H264PreviewGenerator.h"
#include "Keyframe.h"
extern "C"
{
    #include <rtp.h>
    #include <debug.h>
}

#pragma region PreviewGenerator
std::vector<uint8_t> H264PreviewGenerator::GenerateJpegImage(const Keyframe& keyframe)
{
    JANUS_LOG(LOG_INFO,
        "FTL: Decoding %d keyframe packets into frame...\n",
        keyframe.rtpPackets.size());
    std::vector<char> keyframeDataBuffer;

    // We need to shove all of the keyframe NAL units into a buffer to feed into libav
    for (const auto& packet : keyframe.rtpPackets)
    {
        // Grab the payload out of the RTP packet
        int payloadLength = 0;
        char* payload = 
            janus_rtp_payload(reinterpret_cast<char*>(packet->data()), packet->size(), &payloadLength);
        
        if (!payload) // || payloadLength < 6
        {
            // Invalid packet payload
            continue;
        }

        // Parse out H264 packet data
        uint8_t fragmentType = *(payload)   & 0b00011111; // 0x1F
        uint8_t nalType      = *(payload+1) & 0b00011111; // 0x1F
        uint8_t startBit     = *(payload+1) & 0b10000000; // 0x80
        uint8_t endBit       = *(payload+1) & 0b01000000; // 0x40

        JANUS_LOG(LOG_INFO,
            "FTL: Packet size %d - Fragment %u, NAL: %u, Start: %u, End: %u\n",
            payloadLength,
            fragmentType,
            nalType,
            startBit,
            endBit);

        // For fragmented types, start bits are special, they have some extra data in the NAL header
        // that we need to include.
        if ((fragmentType == 28))
        {
            if (startBit)
            {
                JANUS_LOG(LOG_INFO, "FTL: START FRAGMENTED NAL\n");

                // Write the start code
                keyframeDataBuffer.push_back(0x00);
                keyframeDataBuffer.push_back(0x00);
                keyframeDataBuffer.push_back(0x01);

                // Write the re-constructed header
                char firstByte = (*payload & 0b11100000) | (*(payload + 1) & 0b00011111);
                keyframeDataBuffer.push_back(firstByte);
            }

            // Write the rest of the payload
            for (int i = 2; i < payloadLength; ++i)
            {
                keyframeDataBuffer.push_back(payload[i]);
            }
        }
        else
        {
            JANUS_LOG(LOG_INFO, "FTL: WRITE SINGLE NAL\n");

            // Write the start code
            keyframeDataBuffer.push_back(0x00);
            keyframeDataBuffer.push_back(0x00);
            keyframeDataBuffer.push_back(0x01);

            // Write the rest of the payload
            for (int i = 0; i < payloadLength; ++i)
            {
                keyframeDataBuffer.push_back(payload[i]);
            }
        }
        
    }

    // Decode time
    const AVCodec* codec;
    AVCodecContext* context = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    int ret;

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        throw std::runtime_error("Could not find H264 codec!");
    }

    context = avcodec_alloc_context3(codec);
    if (!context)
    {
        throw std::runtime_error("Could not allocate video codec context!");
    }

    if (avcodec_open2(context, codec, nullptr) < 0)
    {
        throw std::runtime_error("Could not open codec!");
    }

    av_init_packet(packet);
    packet->data = reinterpret_cast<uint8_t*>(keyframeDataBuffer.data());
    packet->size = keyframeDataBuffer.size();
    packet->flags |= AV_PKT_FLAG_KEY;

    // So let's decode this packet.
    ret = avcodec_send_packet(context, packet);
    if (ret < 0)
    {
        throw std::runtime_error("Error sending a packet for decoding.");
    }

    ret = avcodec_receive_frame(context, frame);
    if (ret < 0)
    {
        throw std::runtime_error("Error receiving decoded frame.");
    }

    JANUS_LOG(LOG_INFO, "FTL: WE'VE GOT A DECODED FRAME!\n");

    // Now encode it to a JPEG
    std::vector<uint8_t> returnVal = encodeToJpeg(frame);

    // av_frame_free(&frame); // This should be handled by encodeToJpeg
    avcodec_free_context(&context);
    av_packet_free(&packet);

    return returnVal;
}
#pragma endregion

#pragma region Private methods
std::vector<uint8_t> H264PreviewGenerator::encodeToJpeg(AVFrame* frame)
{
    int ret;
    const AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    AVPacket* jpegPacket = av_packet_alloc();
    av_init_packet(jpegPacket);
    if (!jpegCodec)
    {
        throw std::runtime_error("Could not find mjpeg codec!");
    }

    AVCodecContext* jpegCodecContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegCodecContext)
    {
        throw std::runtime_error("Failed to allocated mjpeg codec context!");
    }

    jpegCodecContext->pix_fmt       = AV_PIX_FMT_YUVJ420P;
    jpegCodecContext->height        = frame->height;
    jpegCodecContext->width         = frame->width;
    jpegCodecContext->time_base.num = 1;
    jpegCodecContext->time_base.den = 1000000;

    ret = avcodec_open2(jpegCodecContext, jpegCodec, nullptr);
    if (ret < 0)
    {
        throw std::runtime_error("Couldn't open mjpeg codec!");
    }

    ret = avcodec_send_frame(jpegCodecContext, frame);
    if (ret < 0)
    {
        throw std::runtime_error("Error sending frame to jpeg codec!");
    }

    ret = avcodec_receive_packet(jpegCodecContext, jpegPacket);
    if (ret < 0)
    {
        throw std::runtime_error("Error receiving jpeg packet!");
    }

    std::vector<uint8_t> returnVal(jpegPacket->data, jpegPacket->data + jpegPacket->size);
    av_packet_free(&jpegPacket);
    avcodec_free_context(&jpegCodecContext);
    
    return returnVal;
}
#pragma endregion