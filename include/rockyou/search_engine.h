// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_SEARCH_ENGINE_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_SEARCH_ENGINE_H_

#include <string>

#include "rockyou/status.h"

namespace rockyou {

struct SearchOptions {
  bool case_insensitive = false;
};

Status SearchZip(const std::string& path,
                 const std::string& keyword,
                 const SearchOptions& options);

}

#endif
