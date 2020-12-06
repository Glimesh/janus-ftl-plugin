/**
 * @file FtlTypes.h
 * @author Hayden McAfee(hayden@outlook.com)
 * @brief A few utility type defs for FTL/RTP data
 * @version 0.1
 * @date 2020-08-29
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once
#include <cstdint>

/* FTL data types */
typedef uint32_t ftl_channel_id_t;
typedef uint32_t ftl_stream_id_t;

/* RTP data types */
typedef uint8_t rtp_payload_type_t;
typedef uint16_t rtp_sequence_num_t;
typedef uint32_t rtp_ssrc_t;
typedef uint32_t rtp_timestamp_t;