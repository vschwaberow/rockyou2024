// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_SEARCH_ENGINE_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_SEARCH_ENGINE_H_

#include <optional>
#include <string>

#include "rockyou/status.h"

namespace rockyou {

struct SearchOptions {
  bool case_insensitive = false;
  bool quiet = false;
  bool json = false;
  bool highlight = false;
  std::optional<int> limit;
  std::optional<int> per_file_limit;
  std::optional<unsigned int> thread_count;
  std::optional<size_t> chunk_size;
  std::optional<size_t> context_size;
  std::optional<std::string> checksum;
};

Status SearchZip(const std::string& path,
                 const std::string& keyword,
                 const SearchOptions& options);

}

#endif
