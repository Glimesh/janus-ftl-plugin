/**
 * @file FtlExceptions.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @brief This file contains definitions for all of the FTL exception classes
 * @version 0.1
 * @date 2020-10-16
 * 
 * @copyright Copyright (c) 2020 Hayden McAfee
 * 
 */

#pragma once

#include <stdexcept>

/**
 * @brief Exception describing a failure with preview generation
 */
struct PreviewGenerationFailedException : std::runtime_error
{
    PreviewGenerationFailedException(const char* message) throw() : 
        std::runtime_error(message)
    { }
};

/**
 * @brief Exception describing a failure in communicating to the service connection
 */
struct ServiceConnectionCommunicationFailedException : std::runtime_error
{
    ServiceConnectionCommunicationFailedException(const char* message) throw() : 
        std::runtime_error(message)
    { }
};