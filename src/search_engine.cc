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
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stop_token>
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

// Modern C++26 error handling from the module
using rockyou::AppError;
using rockyou::ErrorCode;
using rockyou::MakeError;
using rockyou::Result;

namespace rockyou {

using rockyou::AppError; // ensure visible inside the namespace for all declarations

namespace {

constexpr size_t kDefaultChunkSize = 1024 * 1024;
constexpr size_t kDefaultMinFileSizeForBuffer = 10 * 1024 * 1024;
constexpr int kDefaultContextSize = 20;

using Occurrence = std::tuple<int, int, std::string>;

struct SearchResult {
  std::string filename;
  std::vector<Occurrence> occurrences;
  bool truncated = false;
  std::string error;
};

void LowerInPlace(std::string* value) {
  std::transform(value->begin(), value->end(), value->begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

std::vector<size_t> BoyerMoore(std::string_view text, std::string_view pattern) {
  std::vector<size_t> results;
  const int m = static_cast<int>(pattern.length());
  const int n = static_cast<int>(text.length());
  if (m == 0 || n == 0 || m > n) {
    return results;
  }
  std::array<int, 256> bad_char;
  bad_char.fill(-1);
  for (int i = 0; i < m; ++i) {
    bad_char[static_cast<unsigned char>(pattern[i])] = i;
  }
  int shift = 0;
  while (shift <= n - m) {
    int j = m - 1;
    while (j >= 0 && pattern[j] == text[shift + j]) {
      --j;
    }
    if (j < 0) {
      results.push_back(static_cast<size_t>(shift));
      shift += (shift + m < n) ? m - bad_char[static_cast<unsigned char>(text[shift + m])] : 1;
    } else {
      shift += std::max(1, j - bad_char[static_cast<unsigned char>(text[shift + j])]);
    }
  }
  return results;
}

std::vector<size_t> SearchPositions(std::string_view text, std::string_view pattern, bool case_insensitive) {
  if (!case_insensitive) {
    return BoyerMoore(text, pattern);
  }
  std::string lowered(text);
  LowerInPlace(&lowered);
  return BoyerMoore(lowered, pattern);
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
  json.append("{\"total\":");
  json.append(std::to_string(total_occurrences));
  json.append(",\"truncated\":");
  json.append(truncated ? "true" : "false");
  json.append(",\"partial_failure\":");
  json.append(partial_failure ? "true" : "false");
  json.append(",\"params\":{");
  json.append("\"case_insensitive\":");
  json.append(options.case_insensitive ? "true" : "false");
  json.append(",\"quiet\":");
  json.append(options.quiet ? "true" : "false");
  json.append(",\"limit\":");
  if (options.limit.has_value()) {
    json.append(std::to_string(options.limit.value()));
  } else {
    json.append("null");
  }
  json.append(",\"per_file_limit\":");
  if (options.per_file_limit.has_value()) {
    json.append(std::to_string(options.per_file_limit.value()));
  } else {
    json.append("null");
  }
  json.append(",\"threads\":");
  if (options.thread_count.has_value()) {
    json.append(std::to_string(options.thread_count.value()));
  } else {
    json.append("null");
  }
  json.append(",\"chunk\":");
  if (options.chunk_size.has_value()) {
    json.append(std::to_string(options.chunk_size.value()));
  } else {
    json.append("null");
  }
  json.append(",\"context\":");
  if (options.context_size.has_value()) {
    json.append(std::to_string(options.context_size.value()));
  } else {
    json.append("null");
  }
  json.append("},\"results\":[");
  for (size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    json.append("{\"file\":\"");
    json.append(EscapeJson(r.filename));
    json.append("\",\"count\":");
    json.append(std::to_string(r.occurrences.size()));
    json.append(",\"truncated\":");
    json.append(r.truncated ? "true" : "false");
    json.append(",\"occurrences\":[");
    for (size_t j = 0; j < r.occurrences.size(); ++j) {
      const auto& occ = r.occurrences[j];
      json.append("{\"line\":");
      json.append(std::to_string(std::get<0>(occ)));
      json.append(",\"column\":");
      json.append(std::to_string(std::get<1>(occ)));
      json.append(",\"context\":\"");
      json.append(EscapeJson(std::get<2>(occ)));
      json.append("\"}");
      if (j + 1 < r.occurrences.size()) {
        json.push_back(',');
      }
    }
    json.append("}");
    if (!r.error.empty()) {
      json.append(",\"error\":\"");
      json.append(EscapeJson(r.error));
      json.append("\"");
    }
    json.append("}");
    if (i + 1 < results.size()) {
      json.push_back(',');
    }
  }
  json.append("],\"errors\":[");
  for (size_t i = 0; i < errors.size(); ++i) {
    json.append("\"");
    json.append(EscapeJson(errors[i]));
    json.append("\"");
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
  std::array<unsigned char, 8192> buffer{};
  while (file.good()) {
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize read_bytes = file.gcount();
    if (read_bytes > 0) {
      if (SHA256_Update(&ctx, buffer.data(), static_cast<size_t>(read_bytes)) != 1) {
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
  std::array<unsigned char, 8192> buffer{};
  while (file.good()) {
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize read_bytes = file.gcount();
    if (read_bytes > 0) {
      blake3_hasher_update(&hasher, buffer.data(), static_cast<size_t>(read_bytes));
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

std::expected<SearchResult, rockyou::AppError>
SearchFile(const std::string& path, const std::string& name, const ZipIndexEntry& entry, const std::string& keyword,
           std::string_view pattern, bool case_insensitive, size_t chunk_size, size_t context_size,
           size_t min_buffer_size, std::optional<int> per_file_limit, bool highlight) {
  auto stream_or = ZipEntryStream::Create(path, name, entry);
  if (!stream_or) {
    return std::unexpected(stream_or.error());
  }
  ZipEntryStream stream = *std::move(stream_or);
  SearchResult result;
  result.filename = name;
  if (keyword.empty()) {
    return result;
  }
  const size_t keyword_length = keyword.size();
  if (entry.size >= min_buffer_size) {
    std::string buffer(entry.size, '\0');
    size_t total = 0;
    while (total < buffer.size()) {
      const size_t remaining = buffer.size() - total;
      auto read_or = stream.Read(buffer.data() + total, static_cast<unsigned int>(remaining));
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
    const auto positions = SearchPositions(buffer, pattern, case_insensitive);
    for (size_t pos : positions) {
      const auto line_column = ComputeLineColumn(newline_offsets, pos);
      const std::string context = BuildHighlightedContext(buffer, pos, keyword_length, context_size, highlight);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
      if (per_file_limit.has_value() && static_cast<int>(result.occurrences.size()) >= per_file_limit.value()) {
        result.truncated = true;
        break;
      }
    }
    return result;
  }
  std::vector<size_t> newline_offsets;
  std::vector<char> buffer(chunk_size);
  std::string overlap;
  size_t processed = 0;
  while (true) {
    auto read_or = stream.Read(buffer.data(), static_cast<unsigned int>(buffer.size()));
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
    const auto positions = SearchPositions(search_text, pattern, case_insensitive);
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
  auto close_status = stream.CloseWithStatus();
  if (!close_status) {
    return std::unexpected(close_status.error());
  }
  return result;
}

} // namespace

Result<void> SearchZip(const std::string& path, const std::string& keyword, const SearchOptions& options) {
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
  const size_t min_buffer_size = kDefaultMinFileSizeForBuffer;
  const size_t context_size = options.context_size.value_or(static_cast<size_t>(kDefaultContextSize));
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
  std::string pattern_storage = keyword;
  if (options.case_insensitive) {
    LowerInPlace(&pattern_storage);
  }
  const std::string_view pattern = pattern_storage;
  const auto start_time = std::chrono::high_resolution_clock::now();
  unsigned int thread_count = options.thread_count.value_or(std::thread::hardware_concurrency());
  if (thread_count == 0) {
    thread_count = kDefaultThreadCountFallback;
  }
  std::stop_source stop_source;
  std::latch completion_latch{static_cast<std::ptrdiff_t>(thread_count)};
  std::vector<std::jthread> workers;
  workers.reserve(thread_count);
  for ([[maybe_unused]] const auto _ : std::views::iota(0u, thread_count)) {
    workers.emplace_back([&](std::stop_token stop_token) {
      std::stop_callback stop_callback(stop_source.get_token(), [&]() noexcept { stop_source.request_stop(); });
      while (true) {
        if (stop_token.stop_requested()) {
          break;
        }
        const size_t batch_start = next_index.fetch_add(kWorkBatchSize, std::memory_order_relaxed);
        if (batch_start >= entries.size()) {
          break;
        }
        const size_t batch_end = std::min(batch_start + kWorkBatchSize, entries.size());
        for (size_t idx = batch_start; idx < batch_end; ++idx) {
          if (stop_token.stop_requested()) {
            break;
          }
          int slot_budget = remaining_limit.load(std::memory_order_relaxed);
          if (slot_budget <= 0) {
            SearchResult skipped;
            skipped.filename = entries[idx].first;
            skipped.truncated = true;
            results[idx] = std::move(skipped);
            stop_source.request_stop();
            continue;
          }
          const auto& entry = entries[idx];
          auto result_or =
              SearchFile(path, entry.first, entry.second, keyword, pattern, options.case_insensitive, chunk_size,
                         context_size, min_buffer_size, options.per_file_limit, options.highlight);
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
            stop_source.request_stop();
            break;
          }
        }
      }
      completion_latch.count_down();
    });
  }
  completion_latch.wait();
  workers.clear();
  const auto end_time = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double> elapsed = end_time - start_time;

  std::vector<SearchResult> finalized;
  finalized.reserve(results.size());
  int total_occurrences = 0;
  for (auto& slot : results) {
    if (!slot.has_value()) {
      continue;
    }
    total_occurrences += static_cast<int>(slot->occurrences.size());
    finalized.push_back(std::move(slot).value());
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

  if (options.json) {
    std::string json_output =
        BuildJson(finalized, total_after_limit, truncated_global, error_messages, options, partial_failure);
    if (json_output.empty()) {
      return std::unexpected(MakeError(ErrorCode::SearchError, std::string(kJsonSerializationError)));
    }
    std::println("{}", json_output);
  } else if (options.quiet) {
    for (const auto& result : finalized) {
      for (const auto& occurrence : result.occurrences) {
        const int line = std::get<0>(occurrence);
        const int column = std::get<1>(occurrence);
        const std::string& context = std::get<2>(occurrence);
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
      for (const auto& occurrence : result.occurrences) {
        const int line = std::get<0>(occurrence);
        const int column = std::get<1>(occurrence);
        const std::string& context = std::get<2>(occurrence);
        std::println(kOccurrenceDetailFormat, line, column, context);
      }
    }
    std::println(kSearchCompleteFormat, total_after_limit);
    std::println(kTimeTakenFormat, elapsed.count());
    if (truncated_global) {
      std::println("{}", rockyou::kResultsTruncatedMessage);
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
