// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#include "rockyou/pch.h"

#include "rockyou/search_engine.h"

#include "rockyou/zip_reader.h"

namespace rockyou {
namespace {

constexpr size_t kChunkSize = 1024 * 1024;
constexpr size_t kMinFileSizeForBuffer = 10 * 1024 * 1024;
constexpr int kContextSize = 20;

using Occurrence = std::tuple<int, int, std::string>;

struct SearchResult {
  std::string filename;
  std::vector<Occurrence> occurrences;
};

void LowerInPlace(std::string* value) {
  std::transform(value->begin(), value->end(), value->begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
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

std::vector<size_t> SearchPositions(std::string_view text,
                                    const std::string& pattern,
                                    bool case_insensitive) {
  if (!case_insensitive) {
    return BoyerMoore(text, pattern);
  }
  std::string lowered(text);
  LowerInPlace(&lowered);
  return BoyerMoore(lowered, pattern);
}

void AppendNewlines(std::string_view chunk,
                    size_t base_offset,
                    std::vector<size_t>* newline_offsets) {
  for (size_t i = 0; i < chunk.size(); ++i) {
    if (chunk[i] == '\n') {
      newline_offsets->push_back(base_offset + i);
    }
  }
}

std::pair<int, int> ComputeLineColumn(const std::vector<size_t>& newline_offsets,
                                      size_t position) {
  auto it = std::upper_bound(newline_offsets.begin(), newline_offsets.end(), position);
  const size_t count = static_cast<size_t>(it - newline_offsets.begin());
  const int line = static_cast<int>(count + 1);
  if (count == 0) {
    return {line, static_cast<int>(position + 1)};
  }
  const size_t last_break = newline_offsets[count - 1];
  return {line, static_cast<int>(position - last_break)};
}

std::string BuildContext(std::string_view source,
                         size_t match_offset,
                         size_t keyword_length) {
  const size_t start = match_offset > static_cast<size_t>(kContextSize)
                           ? match_offset - static_cast<size_t>(kContextSize)
                           : 0;
  const size_t end = std::min(match_offset + keyword_length + static_cast<size_t>(kContextSize),
                              source.size());
  return std::string(source.substr(start, end - start));
}

StatusOr<SearchResult> SearchFile(const std::string& path,
                                  const std::string& name,
                                  const ZipIndexEntry& entry,
                                  const std::string& keyword,
                                  const std::string& pattern,
                                  bool case_insensitive) {
  auto stream_or = ZipEntryStream::Create(path, name, entry);
  if (!stream_or.ok()) {
    return stream_or.status();
  }
  ZipEntryStream stream = stream_or.TakeValue();
  SearchResult result;
  result.filename = name;
  if (keyword.empty()) {
    return result;
  }
  const size_t keyword_length = keyword.size();
  if (entry.size >= kMinFileSizeForBuffer) {
    std::string buffer(entry.size, '\0');
    size_t total = 0;
    while (total < buffer.size()) {
      const size_t remaining = buffer.size() - total;
      auto read_or = stream.Read(buffer.data() + total,
                                 static_cast<unsigned int>(remaining));
      if (!read_or.ok()) {
        return read_or.status();
      }
      const int read_bytes = read_or.value();
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
      const std::string context = BuildContext(buffer, pos, keyword_length);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
    }
    return result;
  }
  std::vector<size_t> newline_offsets;
  std::vector<char> buffer(kChunkSize);
  std::string overlap;
  size_t processed = 0;
  while (true) {
    auto read_or = stream.Read(buffer.data(), static_cast<unsigned int>(buffer.size()));
    if (!read_or.ok()) {
      return read_or.status();
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
      const std::string context = BuildContext(search_text, pos, keyword_length);
      result.occurrences.emplace_back(line_column.first, line_column.second, context);
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
  return result;
}

}

Status SearchZip(const std::string& path,
                 const std::string& keyword,
                 const SearchOptions& options) {
  if (keyword.empty()) {
    return Status::Error(std::string(kKeywordEmptyError));
  }
  auto index_or = BuildZipIndex(path);
  if (!index_or.ok()) {
    return index_or.status();
  }
  ZipIndex index = index_or.TakeValue();
  std::vector<std::pair<std::string, ZipIndexEntry>> entries;
  entries.reserve(index.size());
  for (const auto& item : index) {
    entries.emplace_back(item.first, item.second);
  }
  if (entries.empty()) {
    std::println(kNoEntriesMessage);
    return Status::Ok();
  }
  std::vector<std::optional<SearchResult>> results(entries.size());
  std::vector<std::string> errors;
  std::mutex errors_mutex;
  std::atomic<size_t> next_index(0);
  std::atomic<int> total_count(0);
  std::string pattern = keyword;
  if (options.case_insensitive) {
    LowerInPlace(&pattern);
  }
  const auto start_time = std::chrono::high_resolution_clock::now();
  unsigned int thread_count = std::thread::hardware_concurrency();
  if (thread_count == 0) {
    thread_count = 4;
  }
  std::vector<std::thread> workers;
  workers.reserve(thread_count);
  for (unsigned int i = 0; i < thread_count; ++i) {
    workers.emplace_back([&, pattern]() {
      while (true) {
        const size_t index_value = next_index.fetch_add(1, std::memory_order_relaxed);
        if (index_value >= entries.size()) {
          break;
        }
        const auto& entry = entries[index_value];
        auto result_or = SearchFile(path,
                                    entry.first,
                                    entry.second,
                                    keyword,
                                    pattern,
                                    options.case_insensitive);
        if (!result_or.ok()) {
          std::lock_guard<std::mutex> lock(errors_mutex);
          errors.push_back(std::format(kErrorProcessingFormat, entry.first, result_or.status().message()));
          continue;
        }
        SearchResult result = result_or.TakeValue();
        total_count.fetch_add(static_cast<int>(result.occurrences.size()), std::memory_order_relaxed);
        results[index_value] = std::move(result);
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  const auto end_time = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double> elapsed = end_time - start_time;
  for (size_t i = 0; i < results.size(); ++i) {
    if (!results[i].has_value()) {
      continue;
    }
    const auto& result = results[i].value();
    std::println(kOccurrencesFormat, result.filename, result.occurrences.size());
    for (const auto& occurrence : result.occurrences) {
      const int line = std::get<0>(occurrence);
      const int column = std::get<1>(occurrence);
      const std::string& context = std::get<2>(occurrence);
      std::println(kOccurrenceDetailFormat, line, column, context);
    }
  }
  std::println(kSearchCompleteFormat, total_count.load(std::memory_order_relaxed));
  std::println(kTimeTakenFormat, elapsed.count());
  if (errors.empty()) {
    return Status::Ok();
  }
  std::string combined;
  for (size_t i = 0; i < errors.size(); ++i) {
    if (i != 0) {
      combined.append("\n");
    }
    combined.append(errors[i]);
  }
  return Status::Error(combined);
}

}
