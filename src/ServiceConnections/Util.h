/**
 * @file Util.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-03-19
 * @copyright Copyright (c) 2021 Daniel Stiner
 * 
 */

#pragma once

#include <httplib.h>

const static time_t DEFAULT_SOCKET_RECEIVE_TIMEOUT_SEC = 1;

static void SetDefaultSocketOptions(socket_t sock)
{
    struct timeval tv;
    tv.tv_sec = DEFAULT_SOCKET_RECEIVE_TIMEOUT_SEC;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<void *>(&yes), sizeof(yes));
};
