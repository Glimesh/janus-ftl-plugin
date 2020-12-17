/**
 * @file Util.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-17
 * @copyright Copyright (c) 2020 Hayden McAfee
 */
#pragma once

#include <iomanip>
#include <sstream>
#include <string>
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
};