// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#include "rockyou/pch.h"

#include "rockyou/terminal_ui.h"

namespace rockyou {

void PrintHeader() {
  const char kAsciiArt[] = R"(
 ____   ___   ____ _  ____   __ ___  _   _ ____   ___ ____  _  _
|  _ \ / _ \ / ___| |/ /\ \ / // _ \| | | |___ \ / _ \___ \| || |
| |_) | | | | |   | ' /  \ V /| | | | | | | __) | | | |__) | || |_
|  _ <| |_| | |___| . \   | | | |_| | |_| |/ __/| |_| / __/|__   _|
|_| \_\\___/ \____|_|\_\  |_|  \___/ \___/|_____|\___/_____|  |_|

© 2024 Volker Schwaberow <volker@schwaberow.de>
Based on rockyou2024 cpp by Mike Madden

)";
  std::print(rockyou::kBannerForeground);
  for (size_t i = 0; kAsciiArt[i] != '\0'; ++i) {
    std::print("{}", kAsciiArt[i]);
  }
  std::print(rockyou::kBannerReset);
  std::println("Version {} ({})", kProjectVersion, kProjectLicense);
  std::println("Author {}", kProjectAuthor);
}

void PrintUsage(const std::string& program_name) {
  std::println(rockyou::kUsageHeader, program_name);
  std::println(rockyou::kUsageInteractive, program_name);
  std::println(rockyou::kUsageOptionsHeader);
  std::println(rockyou::kUsageInteractiveOption);
  std::println(rockyou::kUsageCaseInsensitiveOption);
  std::println(rockyou::kUsageQuietOption);
  std::println(rockyou::kUsageJsonOption);
  std::println(rockyou::kUsageLimitOption);
  std::println(rockyou::kUsagePerFileLimitOption);
  std::println(rockyou::kUsageThreadsOption);
  std::println(rockyou::kUsageChunkOption);
  std::println(rockyou::kUsageContextOption);
  std::println(rockyou::kUsageChecksumOption);
  std::println(rockyou::kUsageHighlightOption);
  std::println(rockyou::kUsageHelpOption);
}

}
