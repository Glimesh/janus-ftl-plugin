/**
 * @file NetworkSocketConnectionTransport.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "NetworkSocketConnectionTransport.h"

#include "../Utilities/Util.h"

#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include <spdlog/fmt/bin_to_hex.h>

#pragma region Constructor/Destructor
NetworkSocketConnectionTransport::NetworkSocketConnectionTransport(
    NetworkSocketConnectionKind kind,
    int socketHandle,
    std::optional<sockaddr_in> targetAddr) : 
    connectionKind(kind),
    socketHandle(socketHandle),
    targetAddr(targetAddr)
{
    
    // First, set the socket to non-blocking IO mode
    int socketFlags = fcntl(socketHandle, F_GETFL, 0);
    if (socketFlags == -1)
    {
        // int error = errno;
        Stop();
        // TODO throw
        // return Result<void>::Error(fmt::format(
        //         "Could not retrieve socket flags. Error {}: {}",
        //         error,
        //         Util::ErrnoToString(error)));
    }

    socketFlags = socketFlags | O_NONBLOCK;
    if (fcntl(socketHandle, F_SETFL, socketFlags) != 0)
    {
        // int error = errno;
        Stop();
        // TODO throw
        // return Result<void>::Error(
        //     fmt::format(
        //         "Could not set socket to non-blocking mode. Error {}: {}",
        //         error,
        //         Util::ErrnoToString(error)));
    }
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

Result<ssize_t> NetworkSocketConnectionTransport::Read(std::vector<std::byte>& buffer)
{
    std::unique_lock lock(readMutex);

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

    poll(pollFds, 1, READ_TIMEOUT_MS);

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
            0,
            reinterpret_cast<sockaddr*>(&recvFromAddr),
            &recvFromAddrLen);

        if (bytesRead == -1)
        {
            buffer.resize(0);

            int error = errno;
            if (error == EINVAL)
            {
                // This means we've closed the socket
                return Result<ssize_t>::Error("Socket is closed");
            }
            else if ((error == EAGAIN) || (error == EWOULDBLOCK))
            {
                // No data was read.
                return Result<ssize_t>::Success(0);
            }
            else
            {
                // Unexpected error!
                return Result<ssize_t>::Error(fmt::format(
                    "Couldn't read from socket. Error {}: {}",
                    error,
                    Util::ErrnoToString(error)));
            }
        }
        else if (bytesRead == 0)
        {
            buffer.resize(bytesRead);

            // Our peer has closed the connection.
            // Unless we're a UDP connection, in which case 0 length is a-okay.
            if (connectionKind != NetworkSocketConnectionKind::Udp)
            {
                return Result<ssize_t>::Error("TCP socket closed, read zero bytes");
            }
            else
            {
                return Result<ssize_t>::Success(0);
            }
        }
        else if (bytesRead > 0)
        {
            buffer.resize(bytesRead);

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
            return Result<ssize_t>::Error("Invalid read");
        }
    }
    else
    {
        // No data available to read
        return Result<ssize_t>::Success(0);
    }
}


Result<void> NetworkSocketConnectionTransport::Write(const std::span<std::byte>& bytes)
{
    std::unique_lock lock(writeMutex);

    if (isStopped)
    {
        return Result<void>::Error("Transport is stopped");
    }

    return sendData(bytes);
}

void NetworkSocketConnectionTransport::Stop()
{
    std::unique_lock readLock(readMutex);
    std::unique_lock writeLock(writeMutex);

    if (!isStopped)
    {
        // We haven't already been asked to close, so shutdown gracefully if possible
        if (socketHandle != 0)
        {
            shutdown(socketHandle, SHUT_RDWR);
            close(socketHandle);
        }
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