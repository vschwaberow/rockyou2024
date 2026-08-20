// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include "unzip.h"

#include <cstddef>
#include <expected>
#include <map>
#include <string>

export module rockyou.zip_reader;

import rockyou.error_handling;

using rockyou::Result;

export namespace rockyou {

struct ZipIndexEntry {
  size_t offset;
  size_t size;
};

using ZipIndex = std::map<std::string, ZipIndexEntry>;

Result<ZipIndex> BuildZipIndex(const std::string& path);

class ZipArchive {
public:
  ZipArchive(const ZipArchive&) = delete;
  ZipArchive& operator=(const ZipArchive&) = delete;
  ZipArchive(ZipArchive&& other) noexcept;
  ZipArchive& operator=(ZipArchive&& other) noexcept;
  ~ZipArchive();

  static Result<ZipArchive> Open(const std::string& path);

  Result<void> OpenEntry(const std::string& name, const ZipIndexEntry& entry);
  std::expected<int, rockyou::AppError> Read(char* buffer, unsigned int length);
  std::expected<void, rockyou::AppError> CloseEntryWithStatus();
  size_t size() const { return size_; }

private:
  explicit ZipArchive(unzFile handle);

  void CloseEntry();

  unzFile handle_;
  size_t size_{0};
  bool entry_open_{false};
};

class ZipEntryStream {
public:
  ZipEntryStream(const ZipEntryStream&) = delete;
  ZipEntryStream& operator=(const ZipEntryStream&) = delete;
  ZipEntryStream(ZipEntryStream&& other) noexcept;
  ZipEntryStream& operator=(ZipEntryStream&& other) noexcept;
  ~ZipEntryStream();

  static Result<ZipEntryStream> Create(const std::string& path, const std::string& name, const ZipIndexEntry& entry);

  std::expected<int, rockyou::AppError> Read(char* buffer, unsigned int length);
  std::expected<void, rockyou::AppError> CloseWithStatus();
  size_t size() const { return size_; }

private:
  ZipEntryStream(unzFile handle, size_t size, bool entry_open);

  Result<void> OpenEntry(const std::string& name, const ZipIndexEntry& entry);
  void CloseEntry();

  unzFile handle_;
  size_t size_;
  bool entry_open_;
};

} // namespace rockyou
