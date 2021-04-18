/**
 * @file UdpConnectionCreator.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-05
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include "UdpConnectionCreator.h"

#include "../ConnectionTransports/NetworkSocketConnectionTransport.h"
#include "../Utilities/Util.h"

#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>

#pragma region ConnectionCreator implementation
std::unique_ptr<ConnectionTransport> UdpConnectionCreator::CreateConnection(
    int port,
    in_addr targetAddr)
{
    // TODO: Allow targeting specific source network interfaces
    // TODO: IPV6 support
    // Create the UDP socket, then hand it to the UdpConnectionTransport
    sockaddr_in socketAddress { 0 };
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port = htons(port);

    int socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == -1)
    {
        int error = errno;
        throw std::runtime_error(fmt::format(
            "Couldn't create UDP socket. Error {}: {}", error, Util::ErrnoToString(error)));
    }

    int bindResult = bind(
        socketHandle,
        reinterpret_cast<sockaddr*>(&socketAddress),
        sizeof(socketAddress));
    if (bindResult != 0)
    {
        int error = errno;
        throw std::runtime_error(fmt::format(
            "Couldn't bind UDP socket. Error {}: {}", error, Util::ErrnoToString(error)));
    }

    sockaddr_in target
    {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = targetAddr,
    };
    auto result = NetworkSocketConnectionTransport::Nonblocking(NetworkSocketConnectionKind::Udp, socketHandle, target);
    if (result.IsError)
    {
        throw std::runtime_error(result.ErrorMessage);
    }
    return std::move(result.Value);
}
#pragma endregion ConnectionCreator implementation