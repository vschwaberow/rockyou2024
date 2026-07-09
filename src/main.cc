// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

#include <print>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

import rockyou.messages;
import rockyou.search_engine;
import rockyou.terminal_ui;

namespace rockyou {}

namespace {

template <typename T> bool ParsePositive(std::string_view text, T* value) {
  T parsed{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || parsed <= 0) {
    return false;
  }
  *value = parsed;
  return true;
}

} // namespace

int main(int argc, char* argv[]) {
  rockyou::PrintHeader();
  rockyou::SearchOptions options;
  std::string keyword;
  std::string filename;
  rockyou::SearchOptions default_options; // for scope
  if (argc == 2 && std::string_view(argv[1]) == rockyou::kInteractiveFlag) {
    std::println("{}", rockyou::kInteractiveHelp);
    while (true) {
      std::print("{}", rockyou::kInteractivePromptAction);
      std::fflush(stdout);
      std::string action;
      if (!std::getline(std::cin, action)) {
        return 1;
      }
      if (action == "q" || action == "Q") {
        std::println("{}", rockyou::kInteractiveExit);
        return 0;
      }
      if (action != "s" && action != "S") {
        std::println("{}", rockyou::kInteractiveInvalidInput);
        continue;
      }
      rockyou::SearchOptions session_options;
      std::print(rockyou::kPromptEnterKeyword);
      std::fflush(stdout);
      if (!std::getline(std::cin, keyword) || keyword.empty()) {
        std::println("{}", rockyou::kInteractiveInvalidInput);
        continue;
      }
      std::print(rockyou::kPromptEnterZip);
      std::fflush(stdout);
      if (!std::getline(std::cin, filename) || filename.empty()) {
        std::println("{}", rockyou::kInteractiveInvalidInput);
        continue;
      }
      std::print(rockyou::kPromptCaseInsensitive);
      std::fflush(stdout);
      std::string response;
      std::getline(std::cin, response);
      session_options.case_insensitive =
          response == rockyou::kResponseYesLower || response == rockyou::kResponseYesUpper;
      std::error_code fs_error;
      const bool exists = std::filesystem::exists(filename, fs_error);
      if (fs_error || !exists) {
        std::print(stderr, rockyou::kFileMissingFormat, filename);
        continue;
      }
      auto res = rockyou::SearchZip(filename, keyword, session_options);
      if (!res) {
        std::print(stderr, rockyou::kErrorFormat, res.error().message);
      }
    }
  } else {
    std::vector<std::string_view> positional;
    for (int i = 1; i < argc; ++i) {
      std::string_view arg = argv[i];
      if (arg == rockyou::kCaseInsensitiveFlag) {
        options.case_insensitive = true;
      } else if (arg == rockyou::kQuietFlag) {
        options.quiet = true;
      } else if (arg == rockyou::kJsonFlag) {
        options.json = true;
      } else if (arg == rockyou::kCountFlag) {
        options.count = true;
      } else if (arg == rockyou::kHighlightFlag) {
        options.highlight = true;
      } else if (arg == rockyou::kRegexFlag) {
        options.regex = true;
      } else if (arg == rockyou::kLimitFlag || arg == rockyou::kPerFileLimitFlag || arg == rockyou::kThreadsFlag ||
                 arg == rockyou::kChunkSizeFlag || arg == rockyou::kContextSizeFlag || arg == rockyou::kChecksumFlag ||
                 arg == rockyou::kRegexModeFlag) {
        if (i + 1 >= argc) {
          rockyou::PrintUsage(argv[0]);
          return 1;
        }
        std::string_view value = argv[i + 1];
        if (arg == rockyou::kLimitFlag) {
          int parsed = 0;
          if (!ParsePositive(value, &parsed)) {
            std::print(stderr, rockyou::kErrorFormat, rockyou::kInvalidLimitError);
            return 1;
          }
          options.limit = parsed;
        } else if (arg == rockyou::kPerFileLimitFlag) {
          int parsed = 0;
          if (!ParsePositive(value, &parsed)) {
            std::print(stderr, rockyou::kErrorFormat, rockyou::kInvalidLimitError);
            return 1;
          }
          options.per_file_limit = parsed;
        } else if (arg == rockyou::kThreadsFlag) {
          unsigned int parsed = 0;
          if (!ParsePositive(value, &parsed)) {
            std::print(stderr, rockyou::kErrorFormat, rockyou::kInvalidThreadCountError);
            return 1;
          }
          options.thread_count = parsed;
        } else if (arg == rockyou::kChunkSizeFlag) {
          size_t parsed = 0;
          if (!ParsePositive(value, &parsed)) {
            std::print(stderr, rockyou::kErrorFormat, rockyou::kInvalidChunkSizeError);
            return 1;
          }
          options.chunk_size = parsed;
        } else if (arg == rockyou::kContextSizeFlag) {
          size_t parsed = 0;
          if (!ParsePositive(value, &parsed)) {
            std::print(stderr, rockyou::kErrorFormat, rockyou::kInvalidContextSizeError);
            return 1;
          }
          options.context_size = parsed;
        } else if (arg == rockyou::kChecksumFlag) {
          options.checksum = std::string(value);
        } else if (arg == rockyou::kRegexModeFlag) {
          options.regex_mode = std::string(value);
        }
        ++i;
      } else if (arg == "--help") {
        rockyou::PrintUsage(argv[0]);
        return 0;
      } else if (!arg.empty() && arg.front() == '-') {
        rockyou::PrintUsage(argv[0]);
        return 1;
      } else {
        positional.push_back(arg);
      }
    }
    if (positional.size() != 2) {
      rockyou::PrintUsage(argv[0]);
      return 1;
    }
    filename = std::string(positional[0]);
    keyword = std::string(positional[1]);
    if (options.quiet && options.json) {
      std::print(stderr, rockyou::kErrorFormat, rockyou::kQuietJsonConflictError);
      return 1;
    }
  }
  std::error_code fs_error;
  const bool exists = std::filesystem::exists(filename, fs_error);
  if (fs_error || !exists) {
    std::print(stderr, rockyou::kFileMissingFormat, filename);
    return 1;
  }
  auto res = rockyou::SearchZip(filename, keyword, options);
  if (!res) {
    std::print(stderr, rockyou::kErrorFormat, res.error().message);
    return 1;
  }
  return 0;
}
