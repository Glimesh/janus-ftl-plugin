/**
 * @file MockConnectionTransport.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-03-18
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#pragma once

#include <list>

#include "../../src/ConnectionTransports/ConnectionTransport.h"

class MockConnectionTransport : public ConnectionTransport
{
public:
    MockConnectionTransport()
    { }

    void InjectReceivedBytes(const std::vector<std::byte>& bytes)
    {
        receivedBytes.emplace_back(bytes);
    }
    
    void InjectReceivedBytes(const std::string& str)
    {
        receivedBytes.emplace_back(Util::StringToByteVector(str));
    }

    void SetOnWrite(std::function<Result<void>(const std::span<std::byte>& bytes)> onWrite)
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

    void Stop() override
    { }

    Result<ssize_t> Read(std::vector<std::byte>& buffer, std::chrono::milliseconds timeout) override
    {
        if (receivedBytes.empty())
        {
            return Result<ssize_t>::Success(0);
        }
        else
        {
            buffer = std::move(receivedBytes.front());
            receivedBytes.pop_front();
            return Result<ssize_t>::Success(buffer.size());
        }
    }

    Result<void> Write(const std::span<std::byte>& bytes) override
    {
        if (onWrite)
        {
            return onWrite(bytes);
        }
        else
        {
            return Result<void>::Error("No mock onWrite function supplied");
        }
    }

private:
    std::list<std::vector<std::byte>> receivedBytes;
    std::function<Result<void>(const std::span<std::byte>& bytes)> onWrite;
};