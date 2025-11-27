// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_PCH_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_PCH_H_

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <format>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "rockyou/messages.h"
namespace rockyou {

inline constexpr std::string_view kProjectName = "rockyou2024";
inline constexpr std::string_view kProjectVersion = "0.3.0";
inline constexpr std::string_view kProjectAuthor = "Volker Schwaberow <volker@schwaberow.de>";
inline constexpr std::string_view kProjectLicense = "MIT";

}

#endif
