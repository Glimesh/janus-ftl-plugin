/**
 * @file MockConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include "../../src/ConnectionTransports/ConnectionTransport.h"

class MockConnectionTransport : public ConnectionTransport
{
public:
    MockConnectionTransport()
    { }

    MockConnectionTransport(std::function<void(const std::vector<std::byte>&)> onWrite,
        std::function<void(const std::vector<std::byte>&)> onBytesReceived) : 
        onWrite(onWrite), onBytesReceived(onBytesReceived)
    { }

    /* Mock methods */
    void InjectReceivedBytes(const std::vector<std::byte>& bytes)
    {
        if (onBytesReceived)
        {
            onBytesReceived(bytes);
        }
    }

    void InjectReceivedBytes(const std::string& str)
    {
        if (onBytesReceived)
        {
            onBytesReceived(std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(str.data()),
                reinterpret_cast<const std::byte*>(str.data()) + str.size()));
        }
    }

    void SetOnWrite(std::function<void(const std::vector<std::byte>&)> onWrite)
    {
        this->onWrite = onWrite;
    }

    /* ConnectionTransport Implementation */
    std::optional<sockaddr_in> GetAddr() override
    {
        return sockaddr_in { 0 };
    }

    std::optional<sockaddr_in6> GetAddr6() override
    {
        return sockaddr_in6 { 0 };
    }

    Result<void> StartAsync() override
    {
        return Result<void>::Success();
    }

    void Stop(bool noBlock = false) override
    { }

    void Write(const std::vector<std::byte>& bytes) override
    {
        if (onWrite)
        {
            onWrite(bytes);
        }
    }

    void SetOnConnectionClosed(std::function<void(void)> onConnectionClosed) override
    { }

    void SetOnBytesReceived(
        std::function<void(const std::vector<std::byte>&)> onBytesReceived) override
    {
        this->onBytesReceived = onBytesReceived;
    }

private:
    std::function<void(const std::vector<std::byte>&)> onWrite;
    std::function<void(const std::vector<std::byte>&)> onBytesReceived;
};