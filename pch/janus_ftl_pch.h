/**
 * @file janus_ftl_pch.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-02-24
 * @copyright Copyright (c) 2021 Hayden McAfee
 * 
 * @brief Precompiled headers for janus_ftl.so - 
 * put common headers here to optimize build speed
 */

// STL
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Libs
#include <fmt/core.h>
#include <httplib.h>
#include <spdlog/spdlog.h>

// Common headers
#include "../src/Utilities/FtlTypes.h"
#include "../src/Utilities/Result.h"