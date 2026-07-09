// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <print>
#include <string>
#include <string_view>

module rockyou.terminal_ui;

import rockyou.messages;

namespace rockyou {

void PrintHeader() {
  constexpr std::string_view kAsciiArt = R"(
 ____   ___   ____ _  ____   __ ___  _   _ ____   ___ ____  _  _
|  _ \ / _ \ / ___| |/ /\ \ / // _ \| | | |___ \ / _ \___ \| || |
| |_) | | | | |   | ' /  \ V /| | | | | | | __) | | | |__) | || |_
|  _ <| |_| | |___| . \   | | | |_| | |_| |/ __/| |_| / __/|__   _|
|_| \_\\___/ \____|_|\_\  |_|  \___/ \___/|_____|\___/_____|  |_|

© 2024 Volker Schwaberow <volker@schwaberow.de>
Based on rockyou2024 cpp by Mike Madden

)";

  std::print("{}{}{}", kBannerForeground, kAsciiArt, kBannerReset);
  std::println("Version {} ({})", kProjectVersion, kProjectLicense);
  std::println("Author {}", kProjectAuthor);
}

void PrintUsage(const std::string& program_name) {
  std::println(kUsageHeader, program_name);
  std::println(kUsageInteractive, program_name);
  std::println(kUsageOptionsHeader);
  std::println(kUsageInteractiveOption);
  std::println(kUsageCaseInsensitiveOption);
  std::println(kUsageQuietOption);
  std::println(kUsageJsonOption);
  std::println(kUsageCountOption);
  std::println(kUsageLimitOption);
  std::println(kUsagePerFileLimitOption);
  std::println(kUsageThreadsOption);
  std::println(kUsageChunkOption);
  std::println(kUsageContextOption);
  std::println(kUsageChecksumOption);
  std::println(kUsageHighlightOption);
  std::println(kUsageRegexOption);
  std::println(kUsageRegexModeOption);
  std::println(kUsageHelpOption);
}

} // namespace rockyou
