/**
 * @file Watchdog.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @version 0.1
 * @date 2021-03-04
 * 
 * @copyright Copyright (c) 2021 Daniel Stiner
 * 
 */

#include "Watchdog.h"

#include <cstdlib>
#include <string>

#if defined(SYSTEMD_WATCHDOG_SUPPORT)
    #include <systemd/sd-daemon.h>    
#endif

#pragma region Public methods

Watchdog::Watchdog(std::chrono::milliseconds serviceConnectionMetadataReportInterval) {
    if (char* watchdogIntervalUsecEnv = std::getenv("WATCHDOG_USEC"))
    {
        enabled = true;
        
        uint64_t watchdogIntervalUsec = std::stoi(watchdogIntervalUsecEnv);
        std::chrono::microseconds watchdogInterval = std::chrono::microseconds(watchdogIntervalUsec);

        if (watchdogInterval / 2 < serviceConnectionMetadataReportInterval) {
            auto watchdogMs = std::chrono::duration_cast<std::chrono::milliseconds>(watchdogInterval);
            auto metadataMs = std::chrono::duration_cast<std::chrono::milliseconds>(serviceConnectionMetadataReportInterval);
            spdlog::error(
                "Watchdog interval should be at least twice the metadata reporting interval: {}ms vs {}ms",
                watchdogMs.count(),
                metadataMs.count());
        }
    }
}

void Watchdog::Ready()
{
#if defined(SYSTEMD_WATCHDOG_SUPPORT)
    if (enabled) {
        // See https://www.freedesktop.org/software/systemd/man/sd_notify.html#READY=1
        sd_notify(0, "READY=1");
    }
#endif
}

void Watchdog::IAmAlive()
{
#if defined(SYSTEMD_WATCHDOG_SUPPORT)
    if (enabled) {
        // See https://www.freedesktop.org/software/systemd/man/sd_notify.html#WATCHDOG=1
        sd_notify(0, "WATCHDOG=1");
    }
#endif
}
#pragma endregion
