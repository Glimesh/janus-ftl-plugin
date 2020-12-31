/**
 * @file TcpSocketConnectionTransport.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "TcpSocketConnectionTransport.h"

#include "../Utilities/Util.h"

#include <fcntl.h>
#include <fmt/core.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#pragma region Constructor/Destructor
TcpSocketConnectionTransport::TcpSocketConnectionTransport(int socketHandle) : 
    socketHandle(socketHandle)
{ }
#pragma endregion Constructor/Destructor

#pragma region ConnectionTransport Implementation
Result<void> TcpSocketConnectionTransport::StartAsync()
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
        &TcpSocketConnectionTransport::connectionThreadBody,
        this,
        std::move(connectionThreadEndedPromise));
    connectionThread.detach();
    
    return Result<void>::Success();
}

void TcpSocketConnectionTransport::Stop()
{
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
        if (connectionThreadEndedFuture.valid())
        {
            connectionThreadEndedFuture.wait();
        }
    }
    else if (isStopping && !isStopped)
    {
        // We're already stopping - just wait for the connnection thread to end.
        connectionThreadEndedFuture.wait();
    }
}

void TcpSocketConnectionTransport::Write(const std::vector<std::byte>& bytes)
{
    if (!isStopping && !isStopped)
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        int writeResult = write(writePipeFds[1], bytes.data(), bytes.size());
        if (static_cast<size_t>(writeResult) != bytes.size())
        {
            spdlog::error(
                "Write returned {} result, expected {} bytes.",
                writeResult,
                bytes.size());
            closeConnection();
        }
    }
}

void TcpSocketConnectionTransport::SetOnConnectionClosed(
    std::function<void(void)> onConnectionClosed)
{
    this->onConnectionClosed = onConnectionClosed;
}

void TcpSocketConnectionTransport::SetOnBytesReceived(
    std::function<void(const std::vector<std::byte>&)> onBytesReceived)
{
    this->onBytesReceived = onBytesReceived;
}
#pragma endregion ConnectionTransport Implementation

#pragma region Private methods
void TcpSocketConnectionTransport::connectionThreadBody(
    std::promise<void>&& connectionThreadEndedPromise)
{
    // Indicate when this thread has exited.
    connectionThreadEndedPromise.set_value_at_thread_exit();

    // We're connected. Now wait for input/output.
    char readBuf[BUFFER_SIZE];
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
            int bytesRead = read(socketHandle, readBuf, sizeof(readBuf));
            if (bytesRead == -1)
            {
                int error = errno;
                if (error == EINVAL)
                {
                    // This means we've closed the socket
                    break;
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
        }

        // Data available for writing?
        if (pollFds[1].revents & POLLIN)
        {
            int readResult;
            std::byte writeBuffer[BUFFER_SIZE];
            {
                std::lock_guard<std::mutex> lock(writeMutex);
                readResult = read(writePipeFds[0], writeBuffer, sizeof(writeBuffer));
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
            else if (readResult > 0)
            {
                // we have new data to write out to the socket!
                int writeError = 0;
                int bytesWritten = 0;
                while (true)
                {
                    int writeResult = write(
                        socketHandle,
                        (reinterpret_cast<char*>(writeBuffer) + bytesWritten),
                        (readResult - bytesWritten));

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
                            writeError = error;
                            break;
                        }
                    }
                    else if (writeResult < readResult)
                    {
                        // We've written some bytes... but not all. So keep writing.
                        bytesWritten += writeResult;
                        continue;
                    }
                    else
                    {
                        // We've written all the bytes!
                        break;
                    }
                }
                if (writeError > 0)
                {
                    spdlog::error(
                        "Could not write bytes to socket. Error {}: {}",
                        writeError,
                        Util::ErrnoToString(writeError));
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

void TcpSocketConnectionTransport::closeConnection()
{
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
        if (onConnectionClosed)
        {
            onConnectionClosed();
        }
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
#pragma endregion Private methods