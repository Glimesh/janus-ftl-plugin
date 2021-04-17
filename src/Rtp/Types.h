#pragma once

#include <cstdint>

/*
* Current protocol version.
*/
#define RTP_VERSION    2

#define RTP_SEQ_MOD (1<<16)

/* RTP primitive data types */
typedef uint8_t rtp_payload_type_t;
typedef uint16_t rtp_sequence_num_t;
typedef uint32_t rtp_ssrc_t;
typedef uint32_t rtp_timestamp_t;

/* RTP Types */
struct RtpHeader
{
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t Version:2;
    uint16_t Padding:1;
    uint16_t Extension:1;
    uint16_t CsrcCount:4;
    uint16_t MarkerBit:1;
    uint16_t Type:7;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t CsrcCount:4;
    uint16_t Extension:1;
    uint16_t Padding:1;
    uint16_t Version:2;
    uint16_t Type:7;
    uint16_t MarkerBit:1;
#endif
    uint16_t SequenceNumber;
    uint32_t Timestamp;
    uint32_t Ssrc;
    uint32_t Csrc[16];
};

struct RtpHeaderExtension
{
    uint16_t Type;
    uint16_t Length;
};

enum RtcpType
{
    FIR = 192,
    SR = 200,
    RR = 201,
    SDES = 202,
    BYE = 203,
    APP = 204,
    RTPFB = 205,
    PSFB = 206,
    XR = 207,
};

enum RtcpFeedbackMessageType {
    NACK = 1,
};

// See https://tools.ietf.org/html/rfc3550#section-6.1
struct RtcpHeader
{
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t Version:2;
    uint16_t Padding:1;
    uint16_t Rc:5;
    uint16_t Type:8;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t Rc:5;
    uint16_t Padding:1;
    uint16_t Version:2;
    uint16_t Type:8;
#endif
    uint16_t Length:16;
};

struct RtcpFeedbackPacket
{
    RtcpHeader Header;
    uint32_t Ssrc;
    uint32_t Media;
    char Fci[1];
};

struct RtcpFeedbackPacketNackControlInfo
{
    uint16_t Pid;
    uint16_t Blp;
};
