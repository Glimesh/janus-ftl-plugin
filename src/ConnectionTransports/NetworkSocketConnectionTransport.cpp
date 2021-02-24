/**
 * @file NetworkSocketConnectionTransport.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "NetworkSocketConnectionTransport.h"

#include "../Utilities/Util.h"

#include <fcntl.h>
//#include <fmt/core.h>
#include <poll.h>
// #include <spdlog/spdlog.h>
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
{ }
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

Result<void> NetworkSocketConnectionTransport::StartAsync()
{
    // First, set the socket to non-blocking IO mode
    int socketFlags = fcntl(socketHandle, F_GETFL, 0);
    if (socketFlags == -1)
    {
        int error = errno;
        Stop();
        return Result<void>::Error(
            fmt::format(
                "Could not retrieve socket flags. Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    socketFlags = socketFlags | O_NONBLOCK;
    if (fcntl(socketHandle, F_SETFL, socketFlags) != 0)
    {
        int error = errno;
        Stop();
        return Result<void>::Error(
            fmt::format(
                "Could not set socket to non-blocking mode. Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    // Open pipe used for writing
    if (pipe2(writePipeFds, O_NONBLOCK) != 0)
    {
        int error = errno;
        Stop();
        return Result<void>::Error(
            fmt::format(
                "Could not open pipe for writing. Error {}: {}",
                error,
                Util::ErrnoToString(error)));
    }

    // Now we spin up our new thread to handle I/O
    std::promise<void> connectionThreadEndedPromise;
    connectionThreadEndedFuture = connectionThreadEndedPromise.get_future();
    connectionThread = std::thread(
        &NetworkSocketConnectionTransport::connectionThreadBody,
        this,
        std::move(connectionThreadEndedPromise));
    connectionThread.detach();
    
    return Result<void>::Success();
}

void NetworkSocketConnectionTransport::Stop(bool noBlock)
{
    std::unique_lock stoppingLock(stoppingMutex);
    if (!isStopping && !isStopped)
    {
        // Looks like this connection hasn't stopped yet.
        // Close the sockets and wait for the connection thread to end.
        isStopping = true;
        if (socketHandle != 0)
        {
            shutdown(socketHandle, SHUT_RDWR);
            close(socketHandle);
        }

        // Wait for the connection thread (only if it has actually started)
        if (!noBlock && connectionThreadEndedFuture.valid())
        {
            stoppingLock.unlock(); // Unlock so the connection thread can finish stopping
            connectionThreadEndedFuture.wait();
        }
    }
    else if (isStopping && !isStopped)
    {
        // We're already stopping - just wait for the connnection thread to end.
        if (!noBlock && connectionThreadEndedFuture.valid())
        {
            stoppingLock.unlock(); // Unlock so the connection thread can finish stopping
            connectionThreadEndedFuture.wait();
        }
    }
}

void NetworkSocketConnectionTransport::Write(const std::vector<std::byte>& bytes)
{
    std::unique_lock stoppingLock(stoppingMutex);
    if (!isStopping && !isStopped)
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        size_t writeResult = -1;
        size_t expectedBytesWritten = 0;
        if (connectionKind == NetworkSocketConnectionKind::Udp)
        {
            // UDP is datagram-based, so each write is a distinct datagram
            datagramsPendingWrite.push_back(bytes);
            // write a single byte to indicate that we've got pending data
            const char buf[] = { 0 };
            writeResult = write(writePipeFds[1], &buf[0], 1);
            expectedBytesWritten = 1;
        }
        else
        {
            writeResult = write(writePipeFds[1], bytes.data(), bytes.size());
            expectedBytesWritten = bytes.size();
        }
        
        if (writeResult != expectedBytesWritten)
        {
            spdlog::error(
                "Write returned {} result, expected {} bytes.",
                writeResult,
                bytes.size());
            stoppingLock.unlock(); // Unlock so we can start stopping :)
            closeConnection();
        }
    }
}

void NetworkSocketConnectionTransport::SetOnConnectionClosed(
    std::function<void(void)> onConnectionClosed)
{
    this->onConnectionClosed = onConnectionClosed;
}

void NetworkSocketConnectionTransport::SetOnBytesReceived(
    std::function<void(const std::vector<std::byte>&)> onBytesReceived)
{
    this->onBytesReceived = onBytesReceived;
}
#pragma endregion ConnectionTransport Implementation

#pragma region Private methods
void NetworkSocketConnectionTransport::connectionThreadBody(
    std::promise<void>&& connectionThreadEndedPromise)
{
    // Indicate when this thread has exited.
    connectionThreadEndedPromise.set_value_at_thread_exit();

    // We're connected. Now wait for input/output.
    char readBuf[BUFFER_SIZE] = { 0 };
    while (true)
    {
        pollfd pollFds[]
        {
            // Socket read
            {
                .fd = socketHandle,
                .events = POLLIN,
                .revents = 0,
            },
            // Pending writes
            {
                .fd = writePipeFds[0],
                .events = POLLIN,
                .revents = 0,
            },
        };

        poll(pollFds, 2, 200 /*ms*/);

        // Did the socket get closed?
        if (((pollFds[0].revents & POLLERR) > 0) || 
            ((pollFds[0].revents & POLLHUP) > 0) ||
            ((pollFds[0].revents & POLLNVAL) > 0))
        {
            closeConnection();
            return;
        }

        // Data available for reading?
        if (pollFds[0].revents & POLLIN)
        {
            sockaddr_in recvFromAddr{};
            socklen_t recvFromAddrLen = sizeof(recvFromAddr);
            int bytesRead = recvfrom(
                socketHandle,
                readBuf,
                sizeof(readBuf),
                0,
                reinterpret_cast<sockaddr*>(&recvFromAddr),
                &recvFromAddrLen);
            if (bytesRead == -1)
            {
                int error = errno;
                if (error == EINVAL)
                {
                    // This means we've closed the socket
                    break;
                }
                else if ((error == EAGAIN) || (error == EWOULDBLOCK))
                {
                    // No data was read. Keep trying.
                }
                else
                {
                    // Unexpected error! Close.
                    spdlog::error(
                        "Couldn't read from socket. Error {}: {}",
                        error,
                        Util::ErrnoToString(error));
                    closeConnection();
                    break;
                }
            }
            else if (bytesRead == 0)
            {
                // Our peer has closed the connection.
                // Unless we're a UDP connection, in which case 0 length is a-okay.
                if (connectionKind != NetworkSocketConnectionKind::Udp)
                {
                    closeConnection();
                    return;
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
                    std::vector<std::byte> received(
                        reinterpret_cast<std::byte*>(readBuf),
                        (reinterpret_cast<std::byte*>(readBuf) + bytesRead));
                    if (onBytesReceived)
                    {
                        onBytesReceived(received);
                    }
                }
                else
                {
                    spdlog::warn(
                        "Discarding {} bytes received from unexpected address {}, "
                        "expected {}",
                        bytesRead, Util::AddrToString(recvFromAddr.sin_addr),
                        Util::AddrToString(targetAddr.value().sin_addr));
                }
            }
        }

        // Data available for writing?
        if (pollFds[1].revents & POLLIN)
        {
            int readResult;
            std::list<std::vector<std::byte>> datagrams;
            {
                std::lock_guard<std::mutex> lock(writeMutex);
                datagrams = datagramsPendingWrite; // copy pending datagrams then unlock mutex
                datagramsPendingWrite.clear();
                std::byte writeBuffer[BUFFER_SIZE];
                readResult = read(writePipeFds[0], writeBuffer, sizeof(writeBuffer));

                // If there are no datagrams, then what we've read is our datagram!
                if ((datagrams.size() == 0) && (readResult > 0))
                {
                    datagrams.emplace_back(&writeBuffer[0], (&writeBuffer[0] + readResult));
                }
            }
            if (readResult < 0)
            {
                int error = errno;
                spdlog::error(
                    "Read from write pipe failed. Error {}: {}",
                    error,
                    Util::ErrnoToString(error));
                closeConnection();
                break;
            }
            else
            {
                // we may have new data to write out to the socket!
                bool sendError = false;
                for (const std::vector<std::byte>& datagram : datagrams)
                {
                    Result<void> sendResult = sendData(datagram);
                    if (sendResult.IsError)
                    {
                        sendError = true;
                        spdlog::error("Could not write bytes to socket. Error {}",
                            sendResult.ErrorMessage);
                        break;
                    }
                }
                if (sendError)
                {
                    closeConnection();
                    break;
                }
            }
        }
    }

    if (onConnectionClosed != nullptr)
    {
        onConnectionClosed();
    }
}

Result<void> NetworkSocketConnectionTransport::sendData(const std::vector<std::byte>& data)
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

void NetworkSocketConnectionTransport::closeConnection()
{
    bool fireCallback = false;
    {
        std::lock_guard lock(stoppingMutex);
        if (!isStopping)
        {
            // We haven't been asked to stop, so the connection was closed for another reason.
            isStopping = true;
            if (socketHandle != 0)
            {
                shutdown(socketHandle, SHUT_RDWR);
                close(socketHandle);
            }

            // We only callback when we haven't been explicitly told to stop (to avoid feedback loops)
            fireCallback = true;
        }

        if (!isStopped)
        {
            // Once we reach this point, we know the socket has finished closing.
            // Close our write pipes
            close(writePipeFds[0]);
            close(writePipeFds[1]);
            isStopped = true;
        }
    }
    if (fireCallback && onConnectionClosed)
    {
        onConnectionClosed();
    }
}
#pragma endregion Private methods