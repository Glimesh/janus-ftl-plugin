/**
 * @file FtlClient.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-14
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include "FtlClient.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#pragma region Constructor/Destructor
FtlClient::FtlClient(
    std::string targetHostname,
    ftl_channel_id_t channelId,
    std::vector<std::byte> streamKey) : 
    targetHostname(targetHostname),
    channelId(channelId),
    streamKey(std::move(streamKey))
{ }
#pragma endregion Constructor/Destructor

#pragma region Public methods
Result<void> FtlClient::ConnectAsync()
{
    // Look up hostname
    addrinfo addrHints { 0 };
    addrHints.ai_family = AF_INET; // TODO: IPV6 support
    addrHints.ai_socktype = SOCK_STREAM;
    addrHints.ai_protocol = IPPROTO_TCP;
    addrinfo* addrLookup = nullptr;
    int lookupErr = getaddrinfo(
        targetHostname.c_str(),
        std::to_string(FTL_CONTROL_PORT).c_str(),
        &addrHints,
        &addrLookup);
    if (lookupErr != 0)
    {
        freeaddrinfo(addrLookup);
        return Result<void>::Error("Error looking up hostname");
    }

    // Attempt to open TCP connection
    // TODO: Loop through additional addresses on failure. For now, only try the first one.
    addrinfo targetAddr = *addrLookup;
    int socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int connectErr = connect(socketHandle, targetAddr.ai_addr, targetAddr.ai_addrlen);
    if (connectErr != 0)
    {
        freeaddrinfo(addrLookup);
        return Result<void>::Error("Could not connect to Orchestration service on given host");
    }

    freeaddrinfo(addrLookup);

    // We provide the thread with a promise that it will fulfill once the connection
    // is ready. We block until this promise is fulfilled.
    std::promise<void> connectionReadyPromise;
    std::future<void> connectionReadyFuture = connectionReadyPromise.get_future();
    connectionThread = std::thread(
        &FtlClient::connectionThreadBody,
        this,
        std::move(connectionReadyPromise));
    connectionThread.detach();
    connectionReadyFuture.get();

    return Result<void>::Success();
}

void FtlClient::SetOnClosed(std::function<void()> onClosed)
{
    this->onClosed = onClosed;
}
#pragma endregion Public methods

#pragma region Private methods
void FtlClient::connectionThreadBody(std::promise<void>&& connectionReadyPromise)
{
    connectionReadyPromise.set_value();
}
#pragma endregion