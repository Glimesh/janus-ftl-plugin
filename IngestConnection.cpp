#include "IngestConnection.h"
#include <unistd.h>
extern "C"
{
    #include <debug.h>
}
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <sys/socket.h>

#pragma region Constructor/Destructor
IngestConnection::IngestConnection(
    int connectionHandle,
    std::shared_ptr<CredStore> credStore) : 
    connectionHandle(connectionHandle),
    credStore(credStore)
{ }
#pragma endregion

#pragma region Public methods
void IngestConnection::Start()
{
    connectionThread = std::thread(&IngestConnection::startConnectionThread, this);
    connectionThread.detach();
}

void IngestConnection::Stop()
{
    // TODO: Try to tell the client nicely that we're outta here
    shutdown(connectionHandle, SHUT_RDWR);
    //close(connectionHandle);
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
        if (bytesRead == -1)
        {
            if (errno == EINVAL)
            {
                JANUS_LOG(LOG_INFO, "FTL: Ingest connection is being shut down.\n");
            }
            else
            {
                JANUS_LOG(LOG_INFO, "FTL: Unknown socket read error.\n");
            }
            break;
        }
        else if (bytesRead == 0)
        {
            // TODO: Handle client closing connection
            JANUS_LOG(LOG_INFO, "FTL: Client closed ingest connection.\n");
            return;
        }
        else
        {
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
        }
    }
    JANUS_LOG(LOG_INFO, "FTL: Ingest connection thread terminating\n");
}

void IngestConnection::processCommand(std::string command)
{
    JANUS_LOG(LOG_INFO, "FTL: Processing ingest command: %s\n", command.c_str());
    if (command.compare("HMAC") == 0)
    {
        processHmacCommand();
    }
    else if (command.substr(0,7).compare("CONNECT") == 0)
    {
        processConnectCommand(command);
    }
    else if (std::regex_match(command, attributePattern))
    {
        processAttributeCommand(command);
    }
    else if (command.compare(".") == 0)
    {
        processDotCommand();
    }
}

void IngestConnection::processHmacCommand()
{
    // Calculate a new random hmac payload, then send it.
    // We'll need to print it out as a string of hex bytes (00 - ff)
    std::uniform_int_distribution<uint8_t> uniformDistribution(0x00, 0xFF);
    for (unsigned int i = 0; i < hmacPayload.size(); ++i)
    {
        hmacPayload[i] = uniformDistribution(randomEngine);
    }
    std::string hmacString = byteArrayToHexString(&hmacPayload[0], hmacPayload.size());
    JANUS_LOG(LOG_INFO, "FTL: Sending HMAC payload: %s\n", hmacString.c_str());
    write(connectionHandle, "200 ", 4);
    write(connectionHandle, hmacString.c_str(), hmacString.size());
    write(connectionHandle, "\n", 1);
}

void IngestConnection::processConnectCommand(std::string command)
{
    std::smatch matches;

    if (std::regex_search(command, matches, connectPattern) &&
        (matches.size() >= 3))
    {
        std::string channelIdStr = matches[1].str();
        std::string hmacHashStr = matches[2].str();

        uint32_t channelId = static_cast<uint32_t>(std::stoul(channelIdStr));
        std::vector<uint8_t> hmacHash = hexStringToByteArray(hmacHashStr);

        std::string key = credStore->GetHmacKey(channelId);

        uint8_t buffer[512];
        uint32_t bufferLength;
        HMAC(EVP_sha512(), key.c_str(), key.length(), &hmacPayload[0], hmacPayload.size(), buffer, &bufferLength);

        JANUS_LOG(LOG_INFO, "FTL: Client hash: %s\n", hmacHashStr.c_str());
        JANUS_LOG(LOG_INFO, "FTL: Server hash: %s\n", byteArrayToHexString(&buffer[0], bufferLength).c_str());

        // Do the hashed values match?
        bool match = true;
        if (bufferLength != hmacHash.size())
        {
            match = false;
        }
        else
        {
            for (unsigned int i = 0; i < bufferLength; ++i)
            {
                if (hmacHash.at(i) != buffer[i])
                {
                    match = false;
                    break;
                }
            }
        }

        if (match)
        {
            JANUS_LOG(LOG_INFO, "FTL: Hashes match!\n");
            write(connectionHandle, "200\n", 4);
        }
        else
        {
            JANUS_LOG(LOG_INFO, "FTL: Hashes do not match!\n");
            // TODO: Handle error, disconnect
        }
    }
    else
    {
        // TODO: Handle error, disconnect client
        JANUS_LOG(LOG_INFO, "FTL: Malformed CONNECT request!\n");
    }
}

void IngestConnection::processAttributeCommand(std::string command)
{
    std::smatch matches;

    if (std::regex_match(command, matches, attributePattern) &&
        matches.size() >= 3)
    {
        std::string key = matches[1].str();
        std::string value = matches[2].str();

        attributes[key] = value;

        JANUS_LOG(LOG_INFO, "FTL: Updated attribute `%s`: `%s`\n", key.c_str(), value.c_str());
    }
}

void IngestConnection::processDotCommand()
{

}

std::string IngestConnection::byteArrayToHexString(uint8_t* byteArray, uint32_t length)
{
    std::stringstream returnValue;
    returnValue << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i)
    {
        returnValue << std::setw(2) << static_cast<unsigned>(byteArray[i]);
    }
    return returnValue.str();
}

std::vector<uint8_t> IngestConnection::hexStringToByteArray(std::string hexString)
{
    std::vector<uint8_t> retVal;
    std::stringstream convertStream;

    unsigned int buffer;
    unsigned int offset = 0;
    while (offset < hexString.length()) 
    {
        convertStream.clear();
        convertStream << std::hex << hexString.substr(offset, 2);
        convertStream >> std::hex >> buffer;
        retVal.push_back(static_cast<uint8_t>(buffer));
        offset += 2;
    }

    return retVal;
}
#pragma endregion