/**
 * @file NetworkSocketConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "ConnectionTransport.h"

#include "../Utilities/Result.h"

// #include <functional>
// #include <future>
// #include <list>
// #include <mutex>
// #include <optional>
// #include <thread>
// #include <vector>

enum class NetworkSocketConnectionKind
{
    Tcp = 0,
    Udp,
};

/**
 * @brief ConnectionTransport implementation for a TCP/UDP socket connection
 */
class NetworkSocketConnectionTransport : public ConnectionTransport
{
public:
    /* Constructor/Destructor */
    NetworkSocketConnectionTransport(
        NetworkSocketConnectionKind kind,
        int socketHandle,
        std::optional<sockaddr_in> targetAddr = std::nullopt);

    /* ConnectionTransport Implementation */
    std::optional<sockaddr_in> GetAddr() override;
    std::optional<sockaddr_in6> GetAddr6() override;
    Result<void> StartAsync() override;
    void Stop(bool noBlock = false) override;
    void Write(const std::vector<std::byte>& bytes) override;
    void SetOnConnectionClosed(std::function<void(void)> onConnectionClosed) override;
    void SetOnBytesReceived(
        std::function<void(const std::vector<std::byte>&)> onBytesReceived) override;

private:
    /* Static members */
    static constexpr int BUFFER_SIZE = 2048;

    /* Private fields */
    const NetworkSocketConnectionKind connectionKind;
    const int socketHandle = 0;
    std::optional<sockaddr_in> targetAddr = std::nullopt;
    std::mutex stoppingMutex;
    bool isStopping = false; // Indicates that the socket has been requested to close
    bool isStopped = false;  // Indicates that the socket has finished closing
    std::thread connectionThread;
    std::future<void> connectionThreadEndedFuture;
    std::mutex writeMutex;
    int writePipeFds[2]; // We use pipes to write to the socket via poll
    std::list<std::vector<std::byte>> datagramsPendingWrite;
    // Callbacks
    std::function<void(void)> onConnectionClosed;
    std::function<void(const std::vector<std::byte>&)> onBytesReceived;

    /* Private methods */
    void connectionThreadBody(std::promise<void>&& connectionThreadEndedPromise);
    Result<void> sendData(const std::vector<std::byte>& data);
    void closeConnection();
};