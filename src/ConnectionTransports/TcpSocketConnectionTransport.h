/**
 * @file TcpSocketConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "ConnectionTransport.h"

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

/**
 * @brief ConnectionTransport implementation for a TCP socket connection
 */
class TcpSocketConnectionTransport : public ConnectionTransport
{
public:
    /* Constructor/Destructor */
    TcpSocketConnectionTransport(int socketHandle);

    /* ConnectionTransport Implementation */
    Result<void> StartAsync() override;
    void Stop() override;
    void Write(const std::vector<std::byte>& bytes) override;
    void SetOnConnectionClosed(std::function<void(void)> onConnectionClosed) override;
    void SetOnBytesReceived(
        std::function<void(const std::vector<std::byte>&)> onBytesReceived) override;

private:
    /* Static members */
    static constexpr int BUFFER_SIZE = 512;

    /* Private fields */
    const int socketHandle = 0;
    std::atomic<bool> isStopping { false }; // Indicates that the socket has been requested to close
    std::atomic<bool> isStopped { false };  // Indicates that the socket has finished closing
    std::thread connectionThread;
    std::future<void> connectionThreadEndedFuture;
    std::mutex writeMutex;
    int writePipeFds[2]; // We use pipes to write to the socket via poll
    // Callbacks
    std::function<void(void)> onConnectionClosed;
    std::function<void(const std::vector<std::byte>&)> onBytesReceived;

    /* Private methods */
    void connectionThreadBody(std::promise<void>&& connectionThreadEndedPromise);
    void closeConnection();
};