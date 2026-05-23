// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <print>
#include <string>

export module rockyou.terminal_ui;

import rockyou.messages;

export namespace rockyou {

void PrintHeader();
void PrintUsage(const std::string& program_name);

} // namespace rockyou
