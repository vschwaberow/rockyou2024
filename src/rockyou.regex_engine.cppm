// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <print>
#include <algorithm>
#include <expected>
#include <format>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

export module rockyou.regex_engine;

import rockyou.error_handling;
import rockyou.messages;

export namespace rockyou {

enum class RegexMode : int { kECMAScript = 0, kAwk = 1, kGrep = 2, kEgrep = 3 };

struct RegexPattern {
  std::regex pattern;
  RegexMode mode;
  std::string source;
  size_t min_match_length;
  std::string literal_prefix;

  bool HasLiteralPrefix() const { return !literal_prefix.empty() && literal_prefix.length() >= 3; }
};

struct RegexMatch {
  size_t position;
  size_t length;
  std::string matched_text;
};

Result<RegexPattern> CompileRegexPattern(const std::string& pattern_str, RegexMode mode = RegexMode::kECMAScript);

std::vector<RegexMatch> RegexSearchAll(const RegexPattern& regex_pattern, std::string_view text);

bool RegexSearchAny(const RegexPattern& regex_pattern, std::string_view text);

std::string GetLiteralPrefixForFastPath(const RegexPattern& pattern);

RegexMode ParseRegexMode(std::string_view mode_str);

std::string_view RegexModeToString(RegexMode mode);

} // namespace rockyou

namespace {

std::string ExtractLiteralPrefix(std::string_view pattern) {
  std::string prefix;
  bool escape_next = false;
  bool in_char_class = false;

  for (size_t i = 0; i < pattern.length() && prefix.length() < 32; ++i) {
    char c = pattern[i];

    if (escape_next) {
      prefix += c;
      escape_next = false;
      continue;
    }

    if (c == '\\') {
      escape_next = true;
      continue;
    }

    if (c == '[' && !in_char_class) {
      in_char_class = true;
      continue;
    }

    if (c == ']' && in_char_class) {
      in_char_class = false;
      continue;
    }

    if (in_char_class) {
      continue;
    }

    if (c == '^' || c == '$' || c == '.' || c == '*' || c == '+' || c == '?' || c == '|' || c == '(' || c == ')' ||
        c == '{' || c == '}') {
      break;
    }

    prefix += c;
  }

  return prefix;
}

std::regex::flag_type ConvertRegexMode(rockyou::RegexMode mode) {
  switch (mode) {
    case rockyou::RegexMode::kECMAScript:
      return std::regex::ECMAScript;
    case rockyou::RegexMode::kAwk:
      return std::regex::awk;
    case rockyou::RegexMode::kGrep:
      return std::regex::grep;
    case rockyou::RegexMode::kEgrep:
      return std::regex::egrep;
    default:
      return std::regex::ECMAScript;
  }
}

std::size_t EstimateMinMatchLength(std::string_view pattern) {
  std::size_t min_len = 0;
  bool escape_next = false;
  bool in_char_class = false;

  for (size_t i = 0; i < pattern.length(); ++i) {
    char c = pattern[i];

    if (escape_next) {
      min_len++;
      escape_next = false;
      continue;
    }

    if (c == '\\') {
      escape_next = true;
      continue;
    }

    if (c == '[' && !in_char_class) {
      in_char_class = true;
      continue;
    }

    if (c == ']' && in_char_class) {
      in_char_class = false;
      min_len++;
      continue;
    }

    if (in_char_class) {
      continue;
    }

    switch (c) {
      case '^':
      case '$':
      case '|':
      case '(':
      case ')':
        break;
      case '.':
      case '*':
      case '+':
      case '?':
      case '{':
      case '}':
        break;
      default:
        min_len++;
    }
  }

  return min_len;
}

} // anonymous namespace

namespace rockyou {

Result<RegexPattern> CompileRegexPattern(const std::string& pattern_str, RegexMode mode) {

  if (pattern_str.empty()) {
    return std::unexpected(MakeError(ErrorCode::InvalidInput, std::string(kErrorEmptyRegexPattern)));
  }

  try {
    auto regex_flags = ConvertRegexMode(mode);
    std::regex compiled_pattern(pattern_str, regex_flags);

    auto literal_prefix = ExtractLiteralPrefix(pattern_str);
    auto min_match_length = EstimateMinMatchLength(pattern_str);

    return RegexPattern{.pattern = std::move(compiled_pattern),
                        .mode = mode,
                        .source = pattern_str,
                        .min_match_length = min_match_length,
                        .literal_prefix = std::move(literal_prefix)};
  } catch (const std::regex_error& e) {
    return std::unexpected(
        MakeError(ErrorCode::InvalidInput, std::format(kErrorInvalidRegexPattern, pattern_str, e.what())));
  }
}

std::vector<RegexMatch> RegexSearchAll(const RegexPattern& regex_pattern, std::string_view text) {

  std::vector<RegexMatch> matches;
  std::string str(text);
  std::smatch match_results;

  auto begin = str.cbegin();
  auto end = str.cend();

  while (std::regex_search(begin, end, match_results, regex_pattern.pattern)) {
    if (match_results.empty()) {
      break;
    }

    size_t position = std::distance(str.cbegin(), match_results[0].first);
    size_t length = match_results[0].length();

    matches.push_back(RegexMatch{.position = position, .length = length, .matched_text = match_results.str(0)});

    begin = match_results[0].second;
  }

  return matches;
}

bool RegexSearchAny(const RegexPattern& regex_pattern, std::string_view text) {

  std::string str(text);
  return std::regex_search(str, regex_pattern.pattern);
}

std::string GetLiteralPrefixForFastPath(const RegexPattern& pattern) {
  if (!pattern.HasLiteralPrefix()) {
    return "";
  }

  if (pattern.literal_prefix.length() < 3) {
    return "";
  }

  return pattern.literal_prefix;
}

RegexMode ParseRegexMode(std::string_view mode_str) {
  if (mode_str == "ecmascript" || mode_str == "ECMAScript" || mode_str.empty()) {
    return RegexMode::kECMAScript;
  } else if (mode_str == "awk" || mode_str == "AWK") {
    return RegexMode::kAwk;
  } else if (mode_str == "grep" || mode_str == "GREP") {
    return RegexMode::kGrep;
  } else if (mode_str == "egrep" || mode_str == "EGREP") {
    return RegexMode::kEgrep;
  }
  return RegexMode::kECMAScript;
}

std::string_view RegexModeToString(RegexMode mode) {
  switch (mode) {
    case RegexMode::kECMAScript:
      return "ECMAScript";
    case RegexMode::kAwk:
      return "awk";
    case RegexMode::kGrep:
      return "grep";
    case RegexMode::kEgrep:
      return "egrep";
    default:
      return "ECMAScript";
  }
}

} // namespace rockyou
