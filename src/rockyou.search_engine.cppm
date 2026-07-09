// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <print>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

export module rockyou.search_engine;

import rockyou.error_handling;
import rockyou.messages;

export namespace rockyou {

struct SearchOptions {
  bool case_insensitive = false;
  bool quiet = false;
  bool json = false;
  bool count = false;
  bool highlight = false;
  bool regex = false;
  std::optional<int> limit;
  std::optional<int> per_file_limit;
  std::optional<unsigned int> thread_count;
  std::optional<size_t> chunk_size;
  std::optional<size_t> context_size;
  std::optional<std::string> checksum;
  std::optional<std::string> regex_mode;
};

Result<void> SearchZip(const std::string& path, const std::string& keyword, const SearchOptions& options);

} // namespace rockyou
