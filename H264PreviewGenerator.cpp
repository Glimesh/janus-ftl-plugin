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
    #include <libavcodec/avcodec.h>
    #include <debug.h>
}

#pragma region PreviewGenerator
void H264PreviewGenerator::GenerateImage(const Keyframe& keyframe)
{
    JANUS_LOG(LOG_INFO,
        "FTL: Decoding %d keyframe packets into frame...\n",
        keyframe.rtpPackets.size());
    std::vector<char> keyframeDataBuffer;

    // NALU start code
    // keyframeDataBuffer.push_back(0x00);
    // keyframeDataBuffer.push_back(0x00);
    // keyframeDataBuffer.push_back(0x00);
    // keyframeDataBuffer.push_back(0x01);

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
            // keyframeDataBuffer.push_back(0x00);
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
    // AVCodecParserContext* parser;
    AVCodecContext* context = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    int ret;

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        throw std::runtime_error("Could not find H264 codec!");
    }

    // parser = av_parser_init(codec->id);
    // if (!parser)
    // {
    //     throw std::runtime_error("Could not find H264 parser!");
    // }

    context = avcodec_alloc_context3(codec);
    if (!context)
    {
        throw std::runtime_error("Could not allocate video codec context!");
    }

    if (avcodec_open2(context, codec, nullptr) < 0)
    {
        throw std::runtime_error("Could not open codec!");
    }

    // Parse the buffer we've created
    // NOTE: Maybe we can send this straight to an AVPacket ?
    // ret = av_parser_parse2(
    //     parser,
    //     context,
    //     &packet->data,
    //     &packet->size,
    //     reinterpret_cast<const uint8_t*>(keyframeDataBuffer.data()),
    //     keyframeDataBuffer.size(),
    //     AV_NOPTS_VALUE,
    //     AV_NOPTS_VALUE,
    //     0);
    // if (ret < 0)
    // {
    //     throw std::runtime_error("Error while parsing.");
    // }

    // We should see a packet here.
    // if (packet->size <= 0)
    // {
    //     throw std::runtime_error("Expected H264 packet from keyframe, but found none.");
    // }

    // So let's decode this packet.
    // ret = avcodec_send_packet(context, packet);
    // if (ret < 0)
    // {
    //     throw std::runtime_error("Error sending a packet for decoding.");
    // }

    // ret = avcodec_receive_frame(context, frame);
    // if (ret < 0)
    // {
    //     throw std::runtime_error("Error receiving decoded frame.");
    // }

    av_init_packet(packet);
    packet->data = reinterpret_cast<uint8_t*>(keyframeDataBuffer.data());
    packet->size = keyframeDataBuffer.size();
    packet->flags |= AV_PKT_FLAG_KEY;

    int framefinished = 0;
    ret = avcodec_decode_video2(context, frame, &framefinished, packet);

    if (framefinished <= 0)
    {
        throw std::runtime_error("Error receiving decoded frame.");
    }

    JANUS_LOG(LOG_INFO, "FTL: WE'VE GOT A DECODED FRAME!\n");

    av_frame_free(&frame);
    avcodec_free_context(&context);
    // av_parser_close(parser);
    av_packet_free(&packet);
}
#pragma endregion