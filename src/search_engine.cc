// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <openssl/sha.h>
#include "blake3.h"

#include <print>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <latch>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

module rockyou.search_engine;

import rockyou.messages;
import rockyou.error_handling;
import rockyou.zip_reader;
import rockyou.regex_engine;

// Modern C++26 error handling from the module
using rockyou::AppError;
using rockyou::ErrorCode;
using rockyou::MakeError;
using rockyou::Result;

namespace rockyou {

using rockyou::AppError; // ensure visible inside the namespace for all declarations

namespace {

constexpr size_t kDefaultChunkSize = 1024 * 1024;
constexpr size_t kDefaultMaxInMemoryFileSize = 10 * 1024 * 1024;
constexpr int kDefaultContextSize = 20;
constexpr size_t kRegexUnknownOverlap = 65536;

using Occurrence = std::tuple<int, int, std::string>;

struct SearchResult {
  std::string filename;
  std::vector<Occurrence> occurrences;
  bool truncated = false;
  std::string error;
};

class BoyerMooreMatcher {
public:
  BoyerMooreMatcher(std::string_view pattern, bool case_insensitive) : case_insensitive_(case_insensitive) {
    if (case_insensitive) {
      pattern_.resize(pattern.size());
      std::transform(pattern.begin(), pattern.end(), pattern_.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    } else {
      pattern_ = std::string(pattern);
    }

    bad_char_.fill(-1);
    const auto m = static_cast<std::ptrdiff_t>(pattern_.length());
    for (std::ptrdiff_t i = 0; i < m; ++i) {
      bad_char_[static_cast<unsigned char>(pattern_[static_cast<size_t>(i)])] = i;
    }
  }

  [[nodiscard]] std::vector<size_t> Search(std::string_view text) const {
    if (case_insensitive_) {
      return SearchImpl<true>(text);
    }
    return SearchImpl<false>(text);
  }

  [[nodiscard]] size_t pattern_length() const noexcept { return pattern_.length(); }

private:
  template <bool CaseInsensitive> [[nodiscard]] std::vector<size_t> SearchImpl(std::string_view text) const {
    std::vector<size_t> results;
    const auto m = static_cast<std::ptrdiff_t>(pattern_.length());
    const auto n = static_cast<std::ptrdiff_t>(text.length());
    if (m == 0 || n == 0 || m > n) {
      return results;
    }

    const auto normalize = [](char c) noexcept -> unsigned char {
      const auto uc = static_cast<unsigned char>(c);
      if constexpr (CaseInsensitive) {
        return static_cast<unsigned char>(std::tolower(uc));
      } else {
        return uc;
      }
    };

    std::ptrdiff_t shift = 0;
    while (shift <= n - m) {
      std::ptrdiff_t j = m - 1;
      while (j >= 0 && static_cast<unsigned char>(pattern_[static_cast<size_t>(j)]) ==
                           normalize(text[static_cast<size_t>(shift + j)])) {
        --j;
      }
      if (j < 0) {
        results.push_back(static_cast<size_t>(shift));
        if (shift + m < n) {
          shift += m - bad_char_[normalize(text[static_cast<size_t>(shift + m)])];
        } else {
          shift += 1;
        }
      } else {
        shift += std::max<std::ptrdiff_t>(1, j - bad_char_[normalize(text[static_cast<size_t>(shift + j)])]);
      }
    }
    return results;
  }

  std::string pattern_;
  std::array<std::ptrdiff_t, 256> bad_char_{};
  bool case_insensitive_{false};
};

std::vector<RegexMatch> SearchMatchesHybrid(std::string_view text, const RegexPattern& regex_pattern,
                                            const std::optional<BoyerMooreMatcher>& prefix_matcher) {
  if (!prefix_matcher.has_value()) {
    return RegexSearchAll(regex_pattern, text);
  }

  std::vector<size_t> candidates = prefix_matcher->Search(text);
  std::vector<RegexMatch> verified;
  verified.reserve(candidates.size());

  for (size_t candidate_pos : candidates) {
    size_t scan_start = candidate_pos > 100 ? candidate_pos - 100 : 0;
    size_t scan_end = std::min(candidate_pos + 200, text.length());
    std::string_view scan_window = text.substr(scan_start, scan_end - scan_start);

    auto matches = RegexSearchAll(regex_pattern, scan_window);
    const size_t accept_lo = candidate_pos > 50 ? candidate_pos - 50 : 0;
    const size_t accept_hi = candidate_pos + 150;
    for (const auto& match : matches) {
      size_t absolute_pos = scan_start + match.position;
      if (absolute_pos >= accept_lo && absolute_pos < accept_hi) {
        verified.push_back(
            RegexMatch{.position = absolute_pos, .length = match.length, .matched_text = match.matched_text});
      }
    }
  }

  std::sort(verified.begin(), verified.end(), [](const RegexMatch& a, const RegexMatch& b) {
    if (a.position != b.position) {
      return a.position < b.position;
    }
    return a.length < b.length;
  });
  verified.erase(std::unique(verified.begin(), verified.end(),
                             [](const RegexMatch& a, const RegexMatch& b) {
                               return a.position == b.position && a.length == b.length;
                             }),
                 verified.end());
  return verified;
}

void AppendNewlines(std::string_view chunk, size_t base_offset, std::vector<size_t>* newline_offsets) {
  for (size_t i = 0; i < chunk.size(); ++i) {
    if (chunk[i] == '\n') {
      newline_offsets->push_back(base_offset + i);
    }
  }
}

std::pair<int, int> ComputeLineColumn(const std::vector<size_t>& newline_offsets, size_t position) {
  auto it = std::upper_bound(newline_offsets.begin(), newline_offsets.end(), position);
  const size_t count = static_cast<size_t>(it - newline_offsets.begin());
  const int line = static_cast<int>(count + 1);
  if (count == 0) {
    return {line, static_cast<int>(position + 1)};
  }
  const size_t last_break = newline_offsets[count - 1];
  return {line, static_cast<int>(position - last_break)};
}

std::string BuildContext(std::string_view source, size_t match_offset, size_t keyword_length, size_t context_size) {
  const size_t start = match_offset > context_size ? match_offset - context_size : 0;
  const size_t end = std::min(match_offset + keyword_length + context_size, source.size());
  return std::string(source.substr(start, end - start));
}

std::string BuildHighlightedContext(std::string_view source, size_t match_offset, size_t keyword_length,
                                    size_t context_size, bool highlight) {
  std::string context = BuildContext(source, match_offset, keyword_length, context_size);
  if (!highlight) {
    return context;
  }
  const size_t start = match_offset > context_size ? match_offset - context_size : 0;
  const size_t relative = match_offset - start;
  if (relative > context.size()) {
    return context;
  }
  const size_t highlight_end = std::min(relative + keyword_length, context.size());
  context.insert(highlight_end, "]");
  context.insert(relative, "[");
  return context;
}

std::string EscapeJson(std::string_view input) {
  std::string escaped;
  escaped.reserve(input.size() + 8);
  for (char c : input) {
    switch (c) {
      case '"':
        escaped.append("\\\"");
        break;
      case '\\':
        escaped.append("\\\\");
        break;
      case '\b':
        escaped.append("\\b");
        break;
      case '\f':
        escaped.append("\\f");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  return escaped;
}

std::string BuildJson(const std::vector<SearchResult>& results, int total_occurrences, bool truncated,
                      const std::vector<std::string>& errors, const SearchOptions& options, bool partial_failure) {
  std::string json;
  size_t estimated_size = 256 + errors.size() * 64;
  for (const auto& r : results) {
    estimated_size += 64 + r.filename.size() + r.occurrences.size() * 80;
  }
  json.reserve(estimated_size);

  std::format_to(std::back_inserter(json),
                 "{{\"total\":{},\"truncated\":{},\"partial_failure\":{},\"params\":{{"
                 "\"case_insensitive\":{},\"quiet\":{},",
                 total_occurrences, truncated ? "true" : "false", partial_failure ? "true" : "false",
                 options.case_insensitive ? "true" : "false", options.quiet ? "true" : "false");

  if (options.limit.has_value()) {
    std::format_to(std::back_inserter(json), "\"limit\":{},", *options.limit);
  } else {
    json.append("\"limit\":null,");
  }
  if (options.per_file_limit.has_value()) {
    std::format_to(std::back_inserter(json), "\"per_file_limit\":{},", *options.per_file_limit);
  } else {
    json.append("\"per_file_limit\":null,");
  }
  if (options.thread_count.has_value()) {
    std::format_to(std::back_inserter(json), "\"threads\":{},", *options.thread_count);
  } else {
    json.append("\"threads\":null,");
  }
  if (options.chunk_size.has_value()) {
    std::format_to(std::back_inserter(json), "\"chunk\":{},", *options.chunk_size);
  } else {
    json.append("\"chunk\":null,");
  }
  if (options.context_size.has_value()) {
    std::format_to(std::back_inserter(json), "\"context\":{},", *options.context_size);
  } else {
    json.append("\"context\":null,");
  }
  std::format_to(std::back_inserter(json), "\"regex\":{},\"regex_mode\":\"{}\"}},\"results\":[",
                 options.regex ? "true" : "false",
                 options.regex_mode.has_value() ? EscapeJson(*options.regex_mode) : "");

  for (size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    std::format_to(std::back_inserter(json), "{{\"file\":\"{}\",\"count\":{},\"truncated\":{},\"occurrences\":[",
                   EscapeJson(r.filename), r.occurrences.size(), r.truncated ? "true" : "false");

    for (size_t j = 0; j < r.occurrences.size(); ++j) {
      const auto& [line, column, context] = r.occurrences[j];
      std::format_to(std::back_inserter(json), "{{\"line\":{},\"column\":{},\"context\":\"{}\"}}", line, column,
                     EscapeJson(context));
      if (j + 1 < r.occurrences.size()) {
        json.push_back(',');
      }
    }
    json.append("]");
    if (!r.error.empty()) {
      std::format_to(std::back_inserter(json), ",\"error\":\"{}\"", EscapeJson(r.error));
    }
    json.append("}");
    if (i + 1 < results.size()) {
      json.push_back(',');
    }
  }
  json.append("],\"errors\":[");
  for (size_t i = 0; i < errors.size(); ++i) {
    std::format_to(std::back_inserter(json), "\"{}\"", EscapeJson(errors[i]));
    if (i + 1 < errors.size()) {
      json.push_back(',');
    }
  }
  json.append("]}");
  return json;
}

enum class ChecksumAlgorithm {
  kSha256,
  kBlake3,
  kUnknown,
};

struct ChecksumExpectation {
  ChecksumAlgorithm algorithm = ChecksumAlgorithm::kUnknown;
  std::string hex;
};

std::optional<ChecksumExpectation> ParseChecksumExpectation(const std::optional<std::string>& checksum) {
  if (!checksum.has_value() || checksum->empty()) {
    return std::nullopt;
  }
  ChecksumExpectation expectation;
  const std::string& value = *checksum;
  const auto pos = value.find(':');
  if (pos != std::string::npos) {
    const std::string prefix = value.substr(0, pos);
    const std::string remainder = value.substr(pos + 1);
    if (prefix == "sha256") {
      expectation.algorithm = ChecksumAlgorithm::kSha256;
    } else if (prefix == "blake3") {
      expectation.algorithm = ChecksumAlgorithm::kBlake3;
    }
    expectation.hex = remainder;
  } else {
    expectation.algorithm = ChecksumAlgorithm::kUnknown;
    expectation.hex = value;
  }
  return expectation;
}

std::expected<std::string, rockyou::AppError> ComputeSha256Hex(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::unexpected(MakeError(ErrorCode::ZipError, std::string(kZipOpenError) + path));
  }
  SHA256_CTX ctx;
  if (SHA256_Init(&ctx) != 1) {
    return std::unexpected(MakeError(ErrorCode::ChecksumMismatch, std::string(kChecksumMismatchError)));
  }
  constexpr size_t kHashBufferSize = 1024 * 1024;
  const auto buffer = std::make_unique_for_overwrite<unsigned char[]>(kHashBufferSize);
  while (file.good()) {
    file.read(reinterpret_cast<char*>(buffer.get()), static_cast<std::streamsize>(kHashBufferSize));
    const std::streamsize read_bytes = file.gcount();
    if (read_bytes > 0) {
      if (SHA256_Update(&ctx, buffer.get(), static_cast<size_t>(read_bytes)) != 1) {
        return std::unexpected(MakeError(ErrorCode::ChecksumMismatch, std::string(kChecksumMismatchError)));
      }
    }
  }
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  if (SHA256_Final(digest.data(), &ctx) != 1) {
    return std::unexpected(MakeError(ErrorCode::ChecksumMismatch, std::string(kChecksumMismatchError)));
  }
  static constexpr char kHexDigits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(SHA256_DIGEST_LENGTH * 2);
  for (unsigned char byte : digest) {
    hex.push_back(kHexDigits[(byte >> 4) & 0x0F]);
    hex.push_back(kHexDigits[byte & 0x0F]);
  }
  return hex;
}

std::expected<std::string, AppError> ComputeBlake3Hex(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::unexpected(MakeError(ErrorCode::ZipError, std::string(kZipOpenError) + path));
  }
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  constexpr size_t kHashBufferSize = 1024 * 1024;
  const auto buffer = std::make_unique_for_overwrite<unsigned char[]>(kHashBufferSize);
  while (file.good()) {
    file.read(reinterpret_cast<char*>(buffer.get()), static_cast<std::streamsize>(kHashBufferSize));
    const std::streamsize read_bytes = file.gcount();
    if (read_bytes > 0) {
      blake3_hasher_update(&hasher, buffer.get(), static_cast<size_t>(read_bytes));
    }
  }
  std::array<unsigned char, BLAKE3_OUT_LEN> out{};
  blake3_hasher_finalize(&hasher, out.data(), out.size());
  static constexpr char kHexDigits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(BLAKE3_OUT_LEN * 2);
  for (unsigned char byte : out) {
    hex.push_back(kHexDigits[(byte >> 4) & 0x0F]);
    hex.push_back(kHexDigits[byte & 0x0F]);
  }
  return hex;
}

std::expected<void, rockyou::AppError> ValidateChecksum(const std::string& path,
                                                        const std::optional<std::string>& checksum) {
  auto expectation = ParseChecksumExpectation(checksum);
  if (!expectation.has_value()) {
    return {};
  }
  const auto compare_hex = [&](ChecksumAlgorithm algo, const std::string& expected_hex) -> Result<void> {
    if (algo == ChecksumAlgorithm::kSha256) {
      auto hex_or = ComputeSha256Hex(path);
      if (!hex_or) {
        return std::unexpected(hex_or.error());
      }
      return hex_or.value() == expected_hex
                 ? Result<void>{}
                 : std::unexpected(MakeError(ErrorCode::ChecksumMismatch, std::string(kChecksumMismatchError)));
    }
    if (algo == ChecksumAlgorithm::kBlake3) {
      auto hex_or = ComputeBlake3Hex(path);
      if (!hex_or) {
        return std::unexpected(hex_or.error());
      }
      return hex_or.value() == expected_hex
                 ? Result<void>{}
                 : std::unexpected(MakeError(ErrorCode::ChecksumMismatch, std::string(kChecksumMismatchError)));
    }
    // Unknown: try both
    auto hex_sha = ComputeSha256Hex(path);
    if (hex_sha && *hex_sha == expected_hex) {
      return {};
    }
    auto hex_blake = ComputeBlake3Hex(path);
    if (hex_blake && *hex_blake == expected_hex) {
      return {};
    }
    return std::unexpected(MakeError(ErrorCode::SearchError, std::string(kChecksumMismatchError)));
  };
  return compare_hex(expectation->algorithm, expectation->hex);
}

std::expected<SearchResult, rockyou::AppError> SearchFile(ZipArchive& archive, const std::string& name,
                                                          const ZipIndexEntry& entry, const std::string& keyword,
                                                          const BoyerMooreMatcher& matcher, size_t chunk_size,
                                                          size_t context_size, size_t max_in_memory_file_size,
                                                          std::optional<int> per_file_limit, bool highlight) {
  auto open_res = archive.OpenEntry(name, entry);
  if (!open_res) {
    return std::unexpected(open_res.error());
  }
  SearchResult result;
  result.filename = name;
  if (keyword.empty()) {
    return result;
  }
  const size_t keyword_length = keyword.size();
  if (entry.size <= max_in_memory_file_size) {
    std::string buffer(entry.size, '\0');
    size_t total = 0;
    while (total < buffer.size()) {
      const size_t remaining = buffer.size() - total;
      auto read_or = archive.Read(buffer.data() + total, static_cast<unsigned int>(remaining));
      if (!read_or) {
        return std::unexpected(read_or.error());
      }
      const int read_bytes = *read_or;
      if (read_bytes == 0) {
        break;
      }
      total += static_cast<size_t>(read_bytes);
    }
    buffer.resize(total);
    std::vector<size_t> newline_offsets;
    AppendNewlines(buffer, 0, &newline_offsets);
    const auto positions = matcher.Search(buffer);
    for (size_t pos : positions) {
      const auto line_column = ComputeLineColumn(newline_offsets, pos);
      const std::string context = BuildHighlightedContext(buffer, pos, keyword_length, context_size, highlight);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
      if (per_file_limit.has_value() && static_cast<int>(result.occurrences.size()) >= per_file_limit.value()) {
        result.truncated = true;
        break;
      }
    }
    auto close_status = archive.CloseEntryWithStatus();
    if (!close_status) {
      return std::unexpected(close_status.error());
    }
    return result;
  }
  std::vector<size_t> newline_offsets;
  std::vector<char> buffer(chunk_size);
  std::string overlap;
  size_t processed = 0;
  while (true) {
    auto read_or = archive.Read(buffer.data(), static_cast<unsigned int>(buffer.size()));
    if (!read_or) {
      return std::unexpected(read_or.error());
    }
    const int read_bytes = read_or.value();
    if (read_bytes == 0) {
      break;
    }
    std::string chunk(buffer.data(), static_cast<size_t>(read_bytes));
    AppendNewlines(chunk, processed, &newline_offsets);
    const size_t prefix_length = overlap.size();
    const size_t base = processed >= prefix_length ? processed - prefix_length : 0;
    std::string search_text = overlap + chunk;
    const auto positions = matcher.Search(search_text);
    const size_t threshold = processed >= prefix_length ? processed - prefix_length : 0;
    for (size_t pos : positions) {
      const size_t absolute = base + pos;
      if (absolute < threshold) {
        continue;
      }
      const auto line_column = ComputeLineColumn(newline_offsets, absolute);
      const std::string context = BuildHighlightedContext(search_text, pos, keyword_length, context_size, highlight);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
      if (per_file_limit.has_value() && static_cast<int>(result.occurrences.size()) >= per_file_limit.value()) {
        result.truncated = true;
        break;
      }
    }
    processed += static_cast<size_t>(read_bytes);
    if (keyword_length <= 1) {
      overlap.clear();
    } else {
      const size_t max_overlap = keyword_length - 1;
      if (search_text.size() <= max_overlap) {
        overlap = search_text;
      } else {
        overlap = search_text.substr(search_text.size() - max_overlap);
      }
    }
  }
  auto close_status = archive.CloseEntryWithStatus();
  if (!close_status) {
    return std::unexpected(close_status.error());
  }
  return result;
}

std::expected<SearchResult, rockyou::AppError>
SearchFileRegex(ZipArchive& archive, const std::string& name, const ZipIndexEntry& entry,
                const RegexPattern& regex_pattern, const std::optional<BoyerMooreMatcher>& prefix_matcher,
                size_t chunk_size, size_t context_size, size_t max_in_memory_file_size,
                std::optional<int> per_file_limit, bool highlight) {
  auto open_res = archive.OpenEntry(name, entry);
  if (!open_res) {
    return std::unexpected(open_res.error());
  }
  SearchResult result;
  result.filename = name;

  if (entry.size <= max_in_memory_file_size) {
    std::string buffer(entry.size, '\0');
    size_t total = 0;
    while (total < buffer.size()) {
      const size_t remaining = buffer.size() - total;
      auto read_or = archive.Read(buffer.data() + total, static_cast<unsigned int>(remaining));
      if (!read_or) {
        return std::unexpected(read_or.error());
      }
      const int read_bytes = *read_or;
      if (read_bytes == 0) {
        break;
      }
      total += static_cast<size_t>(read_bytes);
    }
    buffer.resize(total);
    std::vector<size_t> newline_offsets;
    AppendNewlines(buffer, 0, &newline_offsets);

    const auto matches = SearchMatchesHybrid(buffer, regex_pattern, prefix_matcher);
    for (const auto& match : matches) {
      const auto line_column = ComputeLineColumn(newline_offsets, match.position);
      const std::string context =
          BuildHighlightedContext(buffer, match.position, match.length, context_size, highlight);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
      if (per_file_limit.has_value() && static_cast<int>(result.occurrences.size()) >= per_file_limit.value()) {
        result.truncated = true;
        break;
      }
    }
    auto close_status = archive.CloseEntryWithStatus();
    if (!close_status) {
      return std::unexpected(close_status.error());
    }
    return result;
  }

  std::vector<size_t> newline_offsets;
  std::vector<char> buffer(chunk_size);
  std::string overlap;
  size_t processed = 0;

  while (true) {
    auto read_or = archive.Read(buffer.data(), static_cast<unsigned int>(buffer.size()));
    if (!read_or) {
      return std::unexpected(read_or.error());
    }
    const int read_bytes = read_or.value();
    if (read_bytes == 0) {
      break;
    }

    std::string chunk(buffer.data(), static_cast<size_t>(read_bytes));
    AppendNewlines(chunk, processed, &newline_offsets);

    const size_t prefix_length = overlap.size();
    const size_t base = processed >= prefix_length ? processed - prefix_length : 0;
    std::string search_text = overlap + chunk;

    const auto matches = SearchMatchesHybrid(search_text, regex_pattern, prefix_matcher);
    const size_t threshold = processed >= prefix_length ? processed - prefix_length : 0;

    for (const auto& match : matches) {
      const size_t absolute = base + match.position;
      if (absolute < threshold) {
        continue;
      }
      const auto line_column = ComputeLineColumn(newline_offsets, absolute);
      const std::string context =
          BuildHighlightedContext(search_text, match.position, match.length, context_size, highlight);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
      if (per_file_limit.has_value() && static_cast<int>(result.occurrences.size()) >= per_file_limit.value()) {
        result.truncated = true;
        break;
      }
    }

    processed += static_cast<size_t>(read_bytes);
    size_t max_overlap = 0;
    if (regex_pattern.min_match_length == 0) {
      max_overlap = std::min(chunk_size > 0 ? chunk_size - 1 : 0, kRegexUnknownOverlap);
    } else {
      max_overlap = regex_pattern.min_match_length - 1;
    }
    if (search_text.size() <= max_overlap) {
      overlap = search_text;
    } else {
      overlap = search_text.substr(search_text.size() - max_overlap);
    }
  }

  auto close_status = archive.CloseEntryWithStatus();
  if (!close_status) {
    return std::unexpected(close_status.error());
  }
  return result;
}

} // namespace

Result<void> SearchZip(const std::string& path, const std::string& keyword, const SearchOptions& options) {
  const auto t0 = std::chrono::high_resolution_clock::now();
  if (keyword.empty()) {
    return std::unexpected(MakeError(ErrorCode::InvalidInput, std::string(kKeywordEmptyError)));
  }
  if (options.limit.has_value() && options.limit.value() <= 0) {
    return std::unexpected(MakeError(ErrorCode::InvalidInput, std::string(kInvalidLimitError)));
  }
  if (options.per_file_limit.has_value() && options.per_file_limit.value() <= 0) {
    return std::unexpected(MakeError(ErrorCode::InvalidInput, std::string(kInvalidLimitError)));
  }
  if (options.thread_count.has_value() && options.thread_count.value() == 0) {
    return std::unexpected(MakeError(ErrorCode::InvalidInput, std::string(kInvalidThreadCountError)));
  }
  if (options.chunk_size.has_value() && options.chunk_size.value() == 0) {
    return std::unexpected(MakeError(ErrorCode::SearchError, std::string(kInvalidChunkSizeError)));
  }
  if (options.context_size.has_value() && options.context_size.value() == 0) {
    return std::unexpected(MakeError(ErrorCode::SearchError, std::string(kInvalidContextSizeError)));
  }
  const size_t chunk_size = options.chunk_size.value_or(kDefaultChunkSize);
  const size_t max_in_memory_file_size = kDefaultMaxInMemoryFileSize;
  const size_t context_size = options.context_size.value_or(static_cast<size_t>(kDefaultContextSize));

  std::optional<RegexPattern> regex_pattern;
  if (options.regex) {
    RegexMode mode = RegexMode::kECMAScript;
    if (options.regex_mode.has_value()) {
      mode = ParseRegexMode(options.regex_mode.value());
    }
    auto compiled_or = CompileRegexPattern(keyword, mode, options.case_insensitive);
    if (!compiled_or) {
      return std::unexpected(compiled_or.error());
    }
    regex_pattern = *std::move(compiled_or);
  }

  auto checksum_status = ValidateChecksum(path, options.checksum);
  if (!checksum_status) {
    return std::unexpected(checksum_status.error());
  }
  auto index_or = BuildZipIndex(path);
  if (!index_or) {
    return std::unexpected(index_or.error());
  }
  ZipIndex index = *std::move(index_or);
  std::vector<std::pair<std::string, ZipIndexEntry>> entries;
  entries.reserve(index.size());
  for (const auto& item : index) {
    entries.emplace_back(item.first, item.second);
  }
  if (entries.empty()) {
    std::println(kNoEntriesMessage);
    return {};
  }
  std::vector<std::optional<SearchResult>> results(entries.size());
  std::vector<std::optional<std::string>> errors(entries.size());
  std::atomic<size_t> next_index{0};
  std::atomic<int> remaining_limit{options.limit.has_value() ? options.limit.value() : std::numeric_limits<int>::max()};
  const BoyerMooreMatcher keyword_matcher(keyword, options.case_insensitive);
  std::optional<BoyerMooreMatcher> regex_prefix_matcher;
  if (regex_pattern.has_value()) {
    std::string prefix = GetLiteralPrefixForFastPath(*regex_pattern);
    if (!prefix.empty() && prefix.length() >= 3) {
      regex_prefix_matcher.emplace(prefix, options.case_insensitive);
    }
  }
  const auto t1 = std::chrono::high_resolution_clock::now();
  unsigned int thread_count = options.thread_count.value_or(std::thread::hardware_concurrency());
  if (thread_count == 0) {
    thread_count = kDefaultThreadCountFallback;
  }
  std::atomic<bool> stop_requested{false};
  std::latch completion_latch{static_cast<std::ptrdiff_t>(thread_count)};
  std::vector<std::thread> workers;
  workers.reserve(thread_count);
  for ([[maybe_unused]] const auto _ : std::views::iota(0u, thread_count)) {
    workers.emplace_back([&]() {
      auto archive_or = ZipArchive::Open(path);
      while (true) {
        if (stop_requested.load(std::memory_order_relaxed)) {
          break;
        }
        const size_t batch_start = next_index.fetch_add(kWorkBatchSize, std::memory_order_relaxed);
        if (batch_start >= entries.size()) {
          break;
        }
        const size_t batch_end = std::min(batch_start + kWorkBatchSize, entries.size());
        for (size_t idx = batch_start; idx < batch_end; ++idx) {
          if (stop_requested.load(std::memory_order_relaxed)) {
            break;
          }
          int slot_budget = remaining_limit.load(std::memory_order_relaxed);
          if (slot_budget <= 0) {
            SearchResult skipped;
            skipped.filename = entries[idx].first;
            skipped.truncated = true;
            results[idx] = std::move(skipped);
            stop_requested.store(true, std::memory_order_relaxed);
            continue;
          }
          const auto& entry = entries[idx];
          std::expected<SearchResult, rockyou::AppError> result_or;
          if (!archive_or) {
            errors[idx] = std::format(kErrorProcessingFormat, entry.first, archive_or.error().message);
            SearchResult failed;
            failed.filename = entry.first;
            failed.error = archive_or.error().message;
            results[idx] = std::move(failed);
            continue;
          }
          ZipArchive& archive = *archive_or;
          if (regex_pattern.has_value()) {
            result_or = SearchFileRegex(archive, entry.first, entry.second, regex_pattern.value(), regex_prefix_matcher,
                                        chunk_size, context_size, max_in_memory_file_size, options.per_file_limit,
                                        options.highlight);
          } else {
            result_or = SearchFile(archive, entry.first, entry.second, keyword, keyword_matcher, chunk_size,
                                   context_size, max_in_memory_file_size, options.per_file_limit, options.highlight);
          }
          if (!result_or) {
            errors[idx] = std::format(kErrorProcessingFormat, entry.first, result_or.error().message);
            SearchResult failed;
            failed.filename = entry.first;
            failed.error = result_or.error().message;
            results[idx] = std::move(failed);
            continue;
          }
          SearchResult result = *std::move(result_or);
          const int found = static_cast<int>(result.occurrences.size());
          results[idx] = std::move(result);
          const int previous_budget = remaining_limit.fetch_sub(found, std::memory_order_relaxed);
          if (previous_budget - found <= 0 && previous_budget > 0) {
            if (idx < results.size() && results[idx].has_value()) {
              results[idx]->truncated = true;
            }
            stop_requested.store(true, std::memory_order_relaxed);
            break;
          }
        }
      }
      completion_latch.count_down();
    });
  }
  completion_latch.wait();
  for (std::thread& worker : workers) {
    worker.join();
  }
  const auto t2 = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double> elapsed = t2 - t1;

  std::vector<SearchResult> finalized;
  finalized.reserve(results.size());
  int total_occurrences = 0;
  size_t bytes_decompressed = 0;
  for (size_t i = 0; i < results.size(); ++i) {
    if (!results[i].has_value()) {
      continue;
    }
    total_occurrences += static_cast<int>(results[i]->occurrences.size());
    bytes_decompressed += entries[i].second.size;
    finalized.push_back(std::move(results[i]).value());
  }

  std::vector<std::string> error_messages;
  error_messages.reserve(errors.size());
  for (auto& slot : errors) {
    if (slot.has_value()) {
      error_messages.push_back(std::move(slot).value());
    }
  }

  for (auto& result : finalized) {
    std::sort(result.occurrences.begin(), result.occurrences.end(), [](const Occurrence& a, const Occurrence& b) {
      if (std::get<0>(a) != std::get<0>(b)) {
        return std::get<0>(a) < std::get<0>(b);
      }
      if (std::get<1>(a) != std::get<1>(b)) {
        return std::get<1>(a) < std::get<1>(b);
      }
      return std::get<2>(a) < std::get<2>(b);
    });
  }

  std::sort(finalized.begin(), finalized.end(),
            [](const SearchResult& a, const SearchResult& b) { return a.filename < b.filename; });

  const size_t limit_value =
      options.limit.has_value() ? static_cast<size_t>(options.limit.value()) : std::numeric_limits<size_t>::max();
  bool truncated_global = false;
  size_t remaining = limit_value;
  int total_after_limit = 0;
  for (auto& result : finalized) {
    if (result.truncated) {
      truncated_global = true;
    }
    if (remaining == 0) {
      if (!result.occurrences.empty()) {
        result.occurrences.clear();
        result.truncated = true;
        truncated_global = true;
      }
      continue;
    }
    const size_t count = result.occurrences.size();
    if (count > remaining) {
      result.occurrences.resize(remaining);
      result.truncated = true;
      truncated_global = true;
    }
    remaining = (count > remaining) ? 0 : remaining - count;
    total_after_limit += static_cast<int>(result.occurrences.size());
  }
  if (!options.limit.has_value()) {
    total_after_limit = total_occurrences;
  }

  const bool partial_failure = !error_messages.empty();

  if (options.count) {
    if (options.json) {
      std::println("{{\"total\":{}}}", total_after_limit);
    } else {
      std::println("{}", total_after_limit);
    }
  } else if (options.json) {
    std::string json_output =
        BuildJson(finalized, total_after_limit, truncated_global, error_messages, options, partial_failure);
    if (json_output.empty()) {
      return std::unexpected(MakeError(ErrorCode::SearchError, std::string(kJsonSerializationError)));
    }
    std::println("{}", json_output);
  } else if (options.quiet) {
    for (const auto& result : finalized) {
      for (const auto& [line, column, context] : result.occurrences) {
        std::println("{}:{}:{}:{}", result.filename, line, column, context);
      }
    }
    if (truncated_global) {
      std::println("{}", rockyou::kResultsTruncatedMessage);
    }
  } else {
    for (const auto& result : finalized) {
      std::println(kOccurrencesFormat, result.filename, result.occurrences.size());
      if (result.truncated) {
        std::println("  {}", rockyou::kResultsTruncatedMessage);
      }
      for (const auto& [line, column, context] : result.occurrences) {
        std::println(kOccurrenceDetailFormat, line, column, context);
      }
    }
    std::println(kSearchCompleteFormat, total_after_limit);
    std::println(kTimeTakenFormat, elapsed.count());
    if (truncated_global) {
      std::println("{}", rockyou::kResultsTruncatedMessage);
    }
  }

  if (options.stats) {
    const auto t3 = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::milli> init_ms = t1 - t0;
    const std::chrono::duration<double, std::milli> search_ms = t2 - t1;
    const std::chrono::duration<double, std::milli> finalize_ms = t3 - t2;
    const std::chrono::duration<double, std::milli> total_ms = t3 - t0;
    const double search_seconds = elapsed.count();
    const double throughput_mbps =
        search_seconds > 0.0 ? static_cast<double>(bytes_decompressed) / (search_seconds * 1024.0 * 1024.0) : 0.0;
    const size_t searched_count = finalized.size();
    const size_t skipped_count = entries.size() - searched_count;

    std::println(stderr, "--- Statistics ---");
    std::println(stderr, "Entries: {} total, {} searched, {} skipped, {} errors", entries.size(), searched_count,
                 skipped_count, error_messages.size());
    std::println(stderr, "Bytes decompressed: {}", bytes_decompressed);
    std::println(stderr, "Timing: init={:.1f}ms search={:.1f}ms finalize={:.1f}ms total={:.1f}ms", init_ms.count(),
                 search_ms.count(), finalize_ms.count(), total_ms.count());
    std::println(stderr, "Throughput: {:.1f} MB/s", throughput_mbps);

    std::vector<std::pair<std::string_view, size_t>> top_entries;
    for (const auto& r : finalized) {
      top_entries.emplace_back(r.filename, r.occurrences.size());
    }
    const size_t top_n = std::min(top_entries.size(), static_cast<size_t>(5));
    std::partial_sort(top_entries.begin(), top_entries.begin() + static_cast<std::ptrdiff_t>(top_n), top_entries.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });
    std::println(stderr, "Top entries by hits:");
    for (size_t i = 0; i < top_n; ++i) {
      std::println(stderr, "  {}. {} ({} hits)", i + 1, top_entries[i].first, top_entries[i].second);
    }
  }

  if (error_messages.empty()) {
    return {};
  }
  std::string combined_errors;
  for (size_t i = 0; i < error_messages.size(); ++i) {
    if (i != 0) {
      combined_errors.append("\n");
    }
    combined_errors.append(error_messages[i]);
  }
  return std::unexpected(MakeError(ErrorCode::SearchError, std::move(combined_errors)));
}

} // namespace rockyou
