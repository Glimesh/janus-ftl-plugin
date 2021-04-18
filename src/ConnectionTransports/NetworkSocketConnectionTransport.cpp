/**
 * @file NetworkSocketConnectionTransport.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "NetworkSocketConnectionTransport.h"

#include "../Utilities/Util.h"

#include <fcntl.h>
#include <fmt/core.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

void closeSocket(int handle)
{
    if (handle != 0)
    {
        shutdown(handle, SHUT_RDWR);
        close(handle);
    }
}

#pragma region Public Members
Result<std::unique_ptr<NetworkSocketConnectionTransport>> NetworkSocketConnectionTransport::Nonblocking(
    NetworkSocketConnectionKind kind,
    int socketHandle,
    std::optional<sockaddr_in> targetAddr)
{
    // First, set the socket to non-blocking IO mode
    int socketFlags = fcntl(socketHandle, F_GETFL, 0);
    if (socketFlags == -1)
    {
        int error = errno;
        closeSocket(socketHandle);
        return Result<std::unique_ptr<NetworkSocketConnectionTransport>>::Error(fmt::format(
                "Could not retrieve socket flags. Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    socketFlags = socketFlags | O_NONBLOCK;
    if (fcntl(socketHandle, F_SETFL, socketFlags) != 0)
    {
        int error = errno;
        closeSocket(socketHandle);
        return Result<std::unique_ptr<NetworkSocketConnectionTransport>>::Error(
            fmt::format(
                "Could not set socket to non-blocking mode. Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    return Result<std::unique_ptr<NetworkSocketConnectionTransport>>::Success(
        std::make_unique<NetworkSocketConnectionTransport>(kind, socketHandle, targetAddr)
    );
}
#pragma endregion Public Members

#pragma region Constructor/Destructor
NetworkSocketConnectionTransport::NetworkSocketConnectionTransport(
    NetworkSocketConnectionKind kind,
    int socketHandle,
    std::optional<sockaddr_in> targetAddr) : 
    connectionKind(kind),
    socketHandle(socketHandle),
    targetAddr(targetAddr)
{
}

NetworkSocketConnectionTransport::~NetworkSocketConnectionTransport()
{
    Stop();
}
#pragma endregion Constructor/Destructor

#pragma region ConnectionTransport Implementation
std::optional<sockaddr_in> NetworkSocketConnectionTransport::GetAddr()
{
    return targetAddr;
}

std::optional<sockaddr_in6> NetworkSocketConnectionTransport::GetAddr6()
{
    // TODO
    return std::nullopt;
}

Result<ssize_t> NetworkSocketConnectionTransport::Read(std::vector<std::byte>& buffer, std::chrono::milliseconds timeout)
{
    std::scoped_lock lock(readMutex);

    if (isStopped)
    {
        return Result<ssize_t>::Error("Transport is stopped");
    }

    pollfd pollFds[]
    {
        // Socket read
        {
            .fd = socketHandle,
            .events = POLLIN,
            .revents = 0,
        },
    };

    poll(pollFds, 1, timeout.count());

    // Did the socket get closed?
    if (((pollFds[0].revents & POLLERR) > 0) || 
        ((pollFds[0].revents & POLLHUP) > 0) ||
        ((pollFds[0].revents & POLLNVAL) > 0))
    {
        return Result<ssize_t>::Error("Socket closed");
    }

    // Data available for reading?
    if (pollFds[0].revents & POLLIN)
    {
        sockaddr_in recvFromAddr{};
        socklen_t recvFromAddrLen = sizeof(recvFromAddr);

        buffer.resize(BUFFER_SIZE);
        ssize_t bytesRead = recvfrom(
            socketHandle,
            buffer.data(),
            buffer.size(),
            MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&recvFromAddr),
            &recvFromAddrLen);

        if (bytesRead == -1)
        {
            int error = errno;
            if (error == EINVAL)
            {
                // This means we've closed the socket
                return Result<ssize_t>::Error("Socket is closed");
            }
            else if ((error == EAGAIN) || (error == EWOULDBLOCK))
            {
                // No data was read.
                buffer.resize(0);
                return Result<ssize_t>::Success(0);
            }
            else
            {
                // Unexpected error!
                buffer.resize(0);
                return Result<ssize_t>::Error(fmt::format(
                    "Couldn't read from socket. Error {}: {}",
                    error,
                    Util::ErrnoToString(error)));
            }
        }
        else if (bytesRead == 0)
        {
            // Our peer has closed the connection.
            // Unless we're a UDP connection, in which case 0 length is a-okay.
            if (connectionKind != NetworkSocketConnectionKind::Udp)
            {
                buffer.resize(0);
                return Result<ssize_t>::Error("TCP socket closed, read zero bytes");
            }
            else
            {
                buffer.resize(0);
                return Result<ssize_t>::Success(0);
            }
        }
        else if (bytesRead > 0)
        {
            bool bytesAreFromExpectedAddr = true;
            if ((connectionKind == NetworkSocketConnectionKind::Udp) && targetAddr.has_value())
            {
                // If we're processing UDP packets, make sure the incoming data is coming
                // from the expected address
                if (recvFromAddr.sin_addr.s_addr == targetAddr.value().sin_addr.s_addr)
                {
                    // Update our outgoing port to match the source
                    // TODO: Synchronize this to make sure we don't write before we know the
                    // correct port.
                    targetAddr.value().sin_port = recvFromAddr.sin_port;
                }
                else
                {
                    bytesAreFromExpectedAddr = false;
                }
            }

            if (bytesAreFromExpectedAddr)
            {
                buffer.resize(bytesRead);
                return Result<ssize_t>::Success(bytesRead);
            }
            else
            {
                spdlog::warn(
                    "Discarding {} bytes received from unexpected address {}, "
                    "expected {}",
                    bytesRead, Util::AddrToString(recvFromAddr.sin_addr),
                    Util::AddrToString(targetAddr.value().sin_addr));
                buffer.resize(0);
                return Result<ssize_t>::Success(0);
            }
        }
        else
        {
            // TODO
            buffer.resize(0);
            return Result<ssize_t>::Error("Invalid read");
        }
    }
    else
    {
        // No data available to read
        buffer.resize(0);
        return Result<ssize_t>::Success(0);
    }
}


Result<void> NetworkSocketConnectionTransport::Write(const std::span<std::byte>& bytes)
{
    std::scoped_lock lock(writeMutex);

    if (isStopped)
    {
        return Result<void>::Error("Transport is stopped");
    }

    return sendData(bytes);
}

void NetworkSocketConnectionTransport::Stop()
{
    std::scoped_lock lock(readMutex, writeMutex);

    if (!isStopped)
    {
        closeSocket(socketHandle);
    }

    // Once we reach this point, we know the socket has finished closing.
    isStopped = true;
}
#pragma endregion ConnectionTransport Implementation

#pragma region Private methods
Result<void> NetworkSocketConnectionTransport::sendData(const std::span<std::byte>& data)
{
    sockaddr_in sendToAddr{};
    sockaddr* sendToAddrPtr = nullptr;
    socklen_t sendToAddrLen = 0;

    if ((connectionKind == NetworkSocketConnectionKind::Udp) &&
        targetAddr.has_value())
    {
        sendToAddr = targetAddr.value();
        sendToAddrPtr = reinterpret_cast<sockaddr*>(&sendToAddr);
        sendToAddrLen = sizeof(sendToAddr);
    }

    size_t bytesWritten = 0;
    while (true)
    {
        size_t writeResult = sendto(
            socketHandle,
            (reinterpret_cast<const char*>(data.data()) + bytesWritten),
            (data.size() - bytesWritten),
            0,
            sendToAddrPtr,
            sendToAddrLen);

        if (writeResult < 0)
        {
            int error = errno;
            if ((error == EAGAIN) || (error == EWOULDBLOCK))
            {
                // Wait for the socket to be ready for writing, then try again.
                pollfd writePollFds[]
                {
                    // Socket write
                    {
                        .fd = socketHandle,
                        .events = POLLOUT,
                        .revents = 0,
                    }
                };
                poll(writePollFds, 1, 200 /*ms*/);
                continue;
            }
            else
            {
                // Unrecoverable error.
                return Result<void>::Error(Util::ErrnoToString(error));
            }
        }
        else if (writeResult < data.size())
        {
            // We've written some bytes... but not all. So keep writing.
            bytesWritten += writeResult;
            continue;
        }
        else
        {
            // We've written all the bytes!
            bytesWritten = writeResult;
            break;
        }
    }

    return Result<void>::Success();
}
#pragma endregion Private methods