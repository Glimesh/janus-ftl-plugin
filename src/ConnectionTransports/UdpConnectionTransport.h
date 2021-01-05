/**
 * @file UdpConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-05
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "ConnectionTransport.h"

#include <atomic>
#include <future>
#include <mutex>
#include <sys/types.h>
#include <thread>

/**
 * @brief Connection transport that operates over a UDP socket connection
 */
class UdpConnectionTransport : public ConnectionTransport
{
public:
    /* Constructor/Destructor */
    UdpConnectionTransport(int socketHandle, sockaddr_in targetAddr);

    /* ConnectionTransport Implementation */
    Result<void> StartAsync() override;
    void Stop() override;
    void Write(const std::vector<std::byte>& bytes) override;
    void SetOnConnectionClosed(std::function<void(void)> onConnectionClosed) override;
    void SetOnBytesReceived(
        std::function<void(const std::vector<std::byte>&)> onBytesReceived) override;

private:
    /* Private fields */
    const int socketHandle = 0;
    const sockaddr_in targetAddr;
    std::atomic<bool> isStopping { false };
    std::atomic<bool> isStopped { false };
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