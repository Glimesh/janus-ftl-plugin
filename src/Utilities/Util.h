/**
 * @file Util.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-17
 * @copyright Copyright (c) 2020 Hayden McAfee
 */
#pragma once

#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>

class Util
{
public:
    static std::vector<std::byte> HexStringToByteArray(std::string hexString)
    {
        std::vector<std::byte> retVal;
        std::stringstream convertStream;

        unsigned int buffer;
        unsigned int offset = 0;
        while (offset < hexString.length()) 
        {
            convertStream.clear();
            convertStream << std::hex << hexString.substr(offset, 2);
            convertStream >> std::hex >> buffer;
            retVal.push_back(static_cast<std::byte>(buffer));
            offset += 2;
        }

        return retVal;
    }

    /**
     * @brief Converts a set of bytes to a string of hex values expressed in ASCII (ex 0001..FF)
     */
    static std::string ByteArrayToHexString(std::byte* byteArray, uint32_t length)
    {
        std::stringstream returnValue;
        returnValue << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < length; ++i)
        {
            returnValue << std::setw(2) << static_cast<unsigned>(byteArray[i]);
        }
        return returnValue.str();
    }

    /**
     * @brief Generates a random binary blob with the given size.
     */
    static std::vector<std::byte> GenerateRandomBinaryPayload(size_t size)
    {
        std::vector<std::byte> payload;
        payload.reserve(size);
        std::uniform_int_distribution<uint8_t> uniformDistribution(0x00, 0xFF);
        for (unsigned int i = 0; i < size; ++i)
        {
            payload[i] = std::byte{ uniformDistribution(randomEngine) };
        }
        return payload;
    }

    /**
     * @brief Given an errno error code, return the string representation.
     */
    static std::string ErrnoToString(int error)
    {
        char errnoStrBuf[256];
        char* errMsg = strerror_r(error, errnoStrBuf, sizeof(errnoStrBuf));
        return std::string(errMsg);
    }

private:
    inline static std::default_random_engine randomEngine { std::random_device()() };
};