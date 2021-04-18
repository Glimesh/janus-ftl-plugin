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
 * @brief Implementation of ConnectionTransport for a TCP/UDP socket connection
 */
class NetworkSocketConnectionTransport : public ConnectionTransport
{
public:
    /* Public Members */
    static constexpr std::chrono::milliseconds DEFAULT_READ_TIMEOUT{200};

    // Factory method that also sets the socket to non-blocking mode
    static Result<std::unique_ptr<NetworkSocketConnectionTransport>> Nonblocking(
        NetworkSocketConnectionKind kind,
        int socketHandle,
        std::optional<sockaddr_in> targetAddr = std::nullopt);

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
    Result<ssize_t> Read(std::vector<std::byte>& bytes, std::chrono::milliseconds timeout) override;
    Result<void> Write(const std::span<std::byte>& bytes) override;

private:
    /* Static members */
    static constexpr int BUFFER_SIZE = 2048;

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