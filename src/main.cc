// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#include "rockyou/pch.h"

#include "rockyou/search_engine.h"
#include "rockyou/terminal_ui.h"

int main(int argc, char* argv[]) {
  rockyou::PrintHeader();
  rockyou::SearchOptions options;
  std::string keyword;
  std::string filename;
  if (argc == 2 && std::strcmp(argv[1], rockyou::kInteractiveFlag) == 0) {
    std::print(rockyou::kPromptEnterKeyword);
    std::fflush(stdout);
    std::getline(std::cin, keyword);
    std::print(rockyou::kPromptEnterZip);
    std::fflush(stdout);
    std::getline(std::cin, filename);
    std::print(rockyou::kPromptCaseInsensitive);
    std::fflush(stdout);
    std::string response;
    std::getline(std::cin, response);
    options.case_insensitive = response == rockyou::kResponseYesLower ||
                               response == rockyou::kResponseYesUpper;
  } else if (argc >= 3 && argc <= 4) {
    filename = argv[1];
    keyword = argv[2];
    if (argc == 4 && std::strcmp(argv[3], rockyou::kCaseInsensitiveFlag) == 0) {
      options.case_insensitive = true;
    }
  } else {
    rockyou::PrintUsage(argv[0]);
    return 1;
  }
  std::error_code fs_error;
  const bool exists = std::filesystem::exists(filename, fs_error);
  if (fs_error || !exists) {
    std::print(stderr, rockyou::kFileMissingFormat, filename);
    return 1;
  }
  const auto status = rockyou::SearchZip(filename, keyword, options);
  if (!status.ok()) {
    std::print(stderr, rockyou::kErrorFormat, status.message());
    return 1;
  }
  return 0;
}
