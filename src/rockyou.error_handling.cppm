// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>

export module rockyou.error_handling;

export namespace rockyou {

enum class ErrorCode { Ok = 0, FileNotFound, ZipError, InvalidInput, ChecksumMismatch, SearchError, Internal };

constexpr std::string_view to_string(ErrorCode code) noexcept { // NOLINT(readability-identifier-naming)
  switch (code) {
    case ErrorCode::Ok:
      return "Ok";
    case ErrorCode::FileNotFound:
      return "File not found";
    case ErrorCode::ZipError:
      return "ZIP error";
    case ErrorCode::InvalidInput:
      return "Invalid input";
    case ErrorCode::ChecksumMismatch:
      return "Checksum mismatch";
    case ErrorCode::SearchError:
      return "Search error";
    case ErrorCode::Internal:
      return "Internal error";
  }
  return "Unknown";
}

struct AppError { // NOLINT(misc-non-private-member-variables-in-classes)
  ErrorCode code{ErrorCode::Internal};
  std::string message{};

  AppError() = default;
  AppError(ErrorCode c, std::string m) : code(c), message(std::move(m)) {}
};

template <typename T> using Result = std::expected<T, AppError>;

// NOLINTNEXTLINE(readability-identifier-naming)
inline AppError OkError() { return {ErrorCode::Ok, {}}; }
// NOLINTNEXTLINE(readability-identifier-naming)
inline AppError MakeError(ErrorCode code, std::string msg) { return {code, std::move(msg)}; }

} // namespace rockyou

// NOLINTNEXTLINE(llvmlibc-implementation-in-namespace)
template <> struct std::formatter<rockyou::AppError> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const rockyou::AppError& err, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "[{}]: {}", rockyou::to_string(err.code), err.message);
  }
};
