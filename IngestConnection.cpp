#include "IngestConnection.h"
#include <unistd.h>
extern "C"
{
    #include <debug.h>
}
#include <sstream>
#include <iomanip>

#pragma region Constructor/Destructor
IngestConnection::IngestConnection(int connectionHandle) : 
    connectionHandle(connectionHandle)
{ }
#pragma endregion

#pragma region Public methods
void IngestConnection::Start()
{
    connectionThread = std::thread(&IngestConnection::startConnectionThread, this);
    connectionThread.detach();
}
#pragma endregion

#pragma region Private methods
void IngestConnection::startConnectionThread()
{
    JANUS_LOG(LOG_INFO, "FTL: Starting ingest connection thread\n");

    char buffer[512];
    std::stringstream cmdStrStream;
    while (true)
    {
        int bytesRead = read(connectionHandle, buffer, sizeof(buffer) - 1);
        unsigned int delimiterCharactersRead = 0;
        for (int i = 0; i < bytesRead; ++i)
        {
            cmdStrStream << buffer[i];
            if (buffer[i] == commandDelimiter.at(delimiterCharactersRead))
            {
                ++delimiterCharactersRead;
                if (delimiterCharactersRead >= commandDelimiter.size())
                {
                    // We've read a command.
                    std::string command = cmdStrStream.str();
                    command = command.substr(0, command.length() - 4); // strip delimiter
                    cmdStrStream.str({});
                    cmdStrStream.clear();
                    delimiterCharactersRead = 0;
                    processCommand(command);
                }
            }
            else
            {
                delimiterCharactersRead = 0;
            }
        }

        if (bytesRead == 0)
        {
            // TODO: Handle client closing connection
            JANUS_LOG(LOG_INFO, "FTL: Client closed ingest connection.\n");
            return;
        }
    }
}

void IngestConnection::processCommand(std::string command)
{
    JANUS_LOG(LOG_INFO, "FTL: Processing ingest command: %s\n", command.c_str());
    if (command.compare("HMAC") == 0)
    {
        processHmacCommand();
    }
    if (command.substr(0,7).compare("CONNECT") == 0)
    {
        processConnectCommand(command);
    }
}

void IngestConnection::processHmacCommand()
{
    // Calculate a new random hmac payload, then send it.
    // We'll need to print it out as a string of hex bytes (00 - ff)
    std::uniform_int_distribution<uint8_t> uniformDistribution(0x00, 0xFF);
    std::stringstream hmacStringStream;
    hmacStringStream << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hmacPayload.size(); ++i)
    {
        hmacPayload[i] = uniformDistribution(randomEngine);
        hmacStringStream << std::setw(2) << static_cast<unsigned>(hmacPayload[i]);
    }
    std::string hmacString = hmacStringStream.str();
    JANUS_LOG(LOG_INFO, "FTL: Sending HMAC payload: %s\n", hmacString.c_str());
    write(connectionHandle, "200 ", 4);
    write(connectionHandle, hmacString.c_str(), hmacString.size());
    write(connectionHandle, "\n", 1);
}

void IngestConnection::processConnectCommand(std::string command)
{
    std::regex connectPattern = std::regex(R"~(CONNECT ([0-9]+) \$([0-9a-f]+))~");
    std::smatch matches;

    if (std::regex_search(command, matches, connectPattern) &&
        (matches.size() >= 3))
    {
        std::string channelId = matches[1].str();
        std::string hmacHash = matches[2].str();

        JANUS_LOG(LOG_INFO, "FTL: Received connect request for channel id %s and HMAC hash %s", channelId.c_str(), hmacHash.c_str());
    }
    else
    {
        // TODO: Handle error, disconnect client
    }
}
#pragma endregion