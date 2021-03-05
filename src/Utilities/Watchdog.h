/**
 * @file Watchdog.h
 * @author Daniel Stiner (danstiner@gmail.com)
 * @version 0.1
 * @date 2021-03-04
 * 
 * @copyright Copyright (c) 2021 Daniel Stiner
 * 
 * Support for watchdogs that can kill this service if it stops responding (e.g. deadlocks)
 * 
 * Currently supports systemd, for context see http://0pointer.de/blog/projects/watchdog.html
 * 
 */

#pragma once

#include <chrono>

class Watchdog
{
public:
    /* Constructor/Destructor */
    Watchdog(std::chrono::milliseconds serviceConnectionMetadataReportInterva);

    /* Public methods */
    void Ready();
    void IAmAlive();

private:
    /* Private fields */
    bool enabled = false;
};
