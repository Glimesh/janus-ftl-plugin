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

#include "../Utilities/Rtp.h"

#pragma region PreviewGenerator
std::vector<uint8_t> H264PreviewGenerator::GenerateJpegImage(
    const std::list<std::vector<std::byte>>& keyframePackets)
{
    AVFramePtr frame = readFramePtr(keyframePackets);

    // Now encode it to a JPEG
    std::vector<uint8_t> returnVal = encodeToJpeg(std::move(frame));
    return returnVal;
}

std::pair<uint16_t, uint16_t> H264PreviewGenerator::ReadStreamDimensions(
    const std::list<std::vector<std::byte>>& keyframePackets)
{
    AVFramePtr frame = readFramePtr(keyframePackets);
    return std::make_pair(frame->width, frame->height);
}
#pragma endregion

#pragma region Private methods
AVFramePtr H264PreviewGenerator::readFramePtr(
    const std::list<std::vector<std::byte>>& keyframePackets)
{
    std::vector<char> keyframeDataBuffer;

    // We need to shove all of the keyframe NAL units into a buffer to feed into libav
    for (const auto& packet : keyframePackets)
    {
        // Grab the payload out of the RTP packet
        const std::span<const std::byte> payload = Rtp::GetRtpPayload(packet);
        if (payload.size() < 2)
        {
            // Invalid packet payload
            continue;
        }

        // Parse out H264 packet data
        uint8_t fragmentType = static_cast<uint8_t>(payload[0]) & 0b00011111; // 0x1F
        // uint8_t nalType      = *(payload+1) & 0b00011111; // 0x1F
        uint8_t startBit = static_cast<uint8_t>(payload[1]) & 0b10000000; // 0x80
        // uint8_t endBit       = *(payload+1) & 0b01000000; // 0x40

        // For fragmented types, start bits are special, they have some extra data in the NAL header
        // that we need to include.
        if (fragmentType == 28)
        {
            if (startBit)
            {
                // Write the start code
                keyframeDataBuffer.push_back(0x00);
                keyframeDataBuffer.push_back(0x00);
                keyframeDataBuffer.push_back(0x01);

                // Write the re-constructed header
                char firstByte = (static_cast<uint8_t>(payload[0]) & 0b11100000) | 
                    (static_cast<uint8_t>(payload[1]) & 0b00011111);
                keyframeDataBuffer.push_back(firstByte);
            }

            // Write the rest of the payload
            for (size_t i = 2; i < payload.size(); ++i)
            {
                keyframeDataBuffer.push_back(static_cast<char>(payload[i]));
            }
        }
        else
        {
            // Write the start code
            keyframeDataBuffer.push_back(0x00);
            keyframeDataBuffer.push_back(0x00);
            keyframeDataBuffer.push_back(0x01);

            // Write the rest of the payload
            for (size_t i = 0; i < payload.size(); ++i)
            {
                keyframeDataBuffer.push_back(static_cast<char>(payload[i]));
            }
        }
        
    }

    // Decode time
    const AVCodec* codec;
    AVFramePtr frame(av_frame_alloc());
    AVPacketPtr packet(av_packet_alloc());
    int ret;

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        throw PreviewGenerationFailedException("Could not find H264 codec!");
    }

    AVCodecContextPtr context(avcodec_alloc_context3(codec));
    if (context.get() == nullptr)
    {
        throw PreviewGenerationFailedException("Could not allocate video codec context!");
    }

    if (avcodec_open2(context.get(), codec, nullptr) < 0)
    {
        throw PreviewGenerationFailedException("Could not open codec!");
    }

    av_init_packet(packet.get());
    packet->data = reinterpret_cast<uint8_t*>(keyframeDataBuffer.data());
    packet->size = keyframeDataBuffer.size();
    packet->flags |= AV_PKT_FLAG_KEY;

    // So let's decode this packet.
    ret = avcodec_send_packet(context.get(), packet.get());
    if (ret < 0)
    {
        throw PreviewGenerationFailedException("Error sending a packet for decoding.");
    }

    // Receive the decoded frame
    ret = avcodec_receive_frame(context.get(), frame.get());
    if (ret < 0)
    {
        throw PreviewGenerationFailedException("Error receiving decoded frame.");
    }

    return frame;
}

std::vector<uint8_t> H264PreviewGenerator::encodeToJpeg(AVFramePtr frame)
{
    int ret;
    const AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    AVPacketPtr jpegPacket(av_packet_alloc());
    av_init_packet(jpegPacket.get());
    if (!jpegCodec)
    {
        throw PreviewGenerationFailedException("Could not find mjpeg codec!");
    }

    AVCodecContextPtr jpegCodecContext(avcodec_alloc_context3(jpegCodec));
    if (jpegCodecContext.get() == nullptr)
    {
        throw PreviewGenerationFailedException("Failed to allocated mjpeg codec context!");
    }

    jpegCodecContext->pix_fmt       = AV_PIX_FMT_YUVJ420P;
    jpegCodecContext->height        = frame->height;
    jpegCodecContext->width         = frame->width;
    jpegCodecContext->time_base.num = 1;
    jpegCodecContext->time_base.den = 1000000;

    ret = avcodec_open2(jpegCodecContext.get(), jpegCodec, nullptr);
    if (ret < 0)
    {
        throw PreviewGenerationFailedException("Couldn't open mjpeg codec!");
    }

    ret = avcodec_send_frame(jpegCodecContext.get(), frame.get());
    if (ret < 0)
    {
        throw PreviewGenerationFailedException("Error sending frame to jpeg codec!");
    }

    ret = avcodec_receive_packet(jpegCodecContext.get(), jpegPacket.get());
    if (ret < 0)
    {
        throw PreviewGenerationFailedException("Error receiving jpeg packet!");
    }

    std::vector<uint8_t> returnVal(jpegPacket->data, jpegPacket->data + jpegPacket->size);
    return returnVal;
}
#pragma endregion