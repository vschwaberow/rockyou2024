// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_PRINT_COMPAT_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_PRINT_COMPAT_H_

#if __has_include(<print>)
#include <print>
#else
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

namespace rockyou::detail {
inline void WriteToStream(std::FILE* stream, std::string_view data) {
  if (stream == nullptr || data.empty()) {
    return;
  }
  std::fwrite(data.data(), 1, data.size(), stream);
}
}  // namespace rockyou::detail

namespace std {

template <typename... Args>
void print(format_string<Args...> fmt, Args&&... args) {
  ::rockyou::detail::WriteToStream(stdout,
                                   std::vformat(fmt.get(), std::make_format_args(args...)));
}

template <typename... Args>
void print(std::FILE* stream, format_string<Args...> fmt, Args&&... args) {
  ::rockyou::detail::WriteToStream(stream,
                                   std::vformat(fmt.get(), std::make_format_args(args...)));
}

template <typename... Args>
void println(format_string<Args...> fmt, Args&&... args) {
  auto rendered = std::vformat(fmt.get(), std::make_format_args(args...));
  rendered.push_back('\n');
  ::rockyou::detail::WriteToStream(stdout, rendered);
}

template <typename... Args>
void println(std::FILE* stream, format_string<Args...> fmt, Args&&... args) {
  auto rendered = std::vformat(fmt.get(), std::make_format_args(args...));
  rendered.push_back('\n');
  ::rockyou::detail::WriteToStream(stream, rendered);
}

inline void println() {
  ::rockyou::detail::WriteToStream(stdout, "\n");
}

}  // namespace std
#endif

#endif
