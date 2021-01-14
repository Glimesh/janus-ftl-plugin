/**
 * @file FtlServer.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-01-14
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

#include "FtlServer.h"

#include "ConnectionListeners/ConnectionListener.h"

#pragma region Constructor/Destructor
FtlServer::FtlServer(
    std::unique_ptr<ConnectionListener> ingestControlListener,
    std::unique_ptr<ConnectionCreator> mediaConnectionCreator) :
    ingestControlListener(std::move(ingestControlListener)),
    mediaConnectionCreator(std::move(mediaConnectionCreator))
{
    this->ingestControlListener->SetOnNewConnection(
        std::bind(&FtlServer::onNewControlConnection, this, std::placeholders::_1));
}
#pragma endregion Constructor/Destructor

#pragma region Public functions
void FtlServer::StartAsync()
{
    // Start listening for new ingest connections
    std::promise<void> listenThreadReadyPromise;
    std::future<void> listenThreadReadyFuture = listenThreadReadyPromise.get_future();
    listenThread = 
        std::thread(&FtlServer::ingestThreadBody, this, std::move(listenThreadReadyPromise));
    listenThread.detach();
    listenThreadReadyFuture.get();
}

void FtlServer::Stop()
{

}
#pragma endregion Public functions

#pragma region Callback setters
void FtlServer::SetOnStreamStarted(StreamStartedCallback onStreamStarted)
{
    this->onStreamStarted = onStreamStarted;
}

void FtlServer::SetOnStreamEnded(StreamEndedCallback onStreamEnded)
{
    this->onStreamEnded = onStreamEnded;
}

void FtlServer::SetOnRtpPacket(RtpPacketCallback onRtpPacket)
{
    this->onRtpPacket = onRtpPacket;
}
#pragma endregion Callback setters

#pragma region Private functions
void FtlServer::ingestThreadBody(std::promise<void>&& readyPromise)
{
    ingestControlListener->Listen(std::move(readyPromise));
}

void FtlServer::onNewControlConnection(std::unique_ptr<ConnectionTransport> connection)
{

}
#pragma endregion Private functions