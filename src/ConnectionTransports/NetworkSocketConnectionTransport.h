/**
 * @file NetworkSocketConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include "ConnectionTransport.h"

#include "../Utilities/Result.h"

#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

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
    virtual ~NetworkSocketConnectionTransport();

    /* ConnectionTransport Implementation */
    std::optional<sockaddr_in> GetAddr() override;
    std::optional<sockaddr_in6> GetAddr6() override;
    void Stop() override;
    Result<ssize_t> Read(std::vector<std::byte>& bytes) override;
    Result<void> Write(const std::span<std::byte>& bytes) override;

private:
    /* Static members */
    static constexpr int BUFFER_SIZE = 2048;
    static constexpr int READ_TIMEOUT_MS = 200;

    /* Private fields */
    const NetworkSocketConnectionKind connectionKind;
    const int socketHandle = 0;
    std::optional<sockaddr_in> targetAddr = std::nullopt;
    bool isStopped = false;
    std::mutex readMutex;
    std::mutex writeMutex;

    /* Private methods */
    Result<void> sendData(const std::span<std::byte>& data);
    void closeConnection();
};