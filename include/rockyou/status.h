// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_STATUS_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_STATUS_H_

#include <optional>
#include <string>
#include <utility>

namespace rockyou {

class Status {
 public:
  static Status Ok();
  static Status Error(std::string message);

  Status(const Status&) = default;
  Status& operator=(const Status&) = default;
  Status(Status&&) noexcept = default;
  Status& operator=(Status&&) noexcept = default;

  bool ok() const;
  const std::string& message() const;

 private:
  Status(bool ok, std::string message);

  bool ok_;
  std::string message_;
};

template <typename T>
class StatusOr {
 public:
  StatusOr(const Status& status) : status_(status) {}
  StatusOr(Status&& status) : status_(std::move(status)) {}
  StatusOr(T&& value) : status_(Status::Ok()), value_(std::move(value)) {}
  StatusOr(const T& value) : status_(Status::Ok()), value_(value) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

  const T& value() const { return *value_; }
  T& value() { return *value_; }

  T TakeValue() {
    T temp = std::move(*value_);
    value_.reset();
    return temp;
  }

 private:
  Status status_;
  std::optional<T> value_;
};

inline Status Status::Ok() {
  return Status(true, {});
}

inline Status Status::Error(std::string message) {
  return Status(false, std::move(message));
}

inline Status::Status(bool ok, std::string message)
    : ok_(ok), message_(std::move(message)) {}

inline bool Status::ok() const {
  return ok_;
}

inline const std::string& Status::message() const {
  return message_;
}

}

#endif
