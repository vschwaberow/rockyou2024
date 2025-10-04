// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_ZIP_READER_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_ZIP_READER_H_

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "rockyou/status.h"
#include "unzip.h"

namespace rockyou {

struct ZipIndexEntry {
  size_t offset;
  size_t size;
};

using ZipIndex = std::map<std::string, ZipIndexEntry>;

StatusOr<ZipIndex> BuildZipIndex(const std::string& path);

class ZipEntryStream {
 public:
  ZipEntryStream(const ZipEntryStream&) = delete;
  ZipEntryStream& operator=(const ZipEntryStream&) = delete;
  ZipEntryStream(ZipEntryStream&& other) noexcept;
  ZipEntryStream& operator=(ZipEntryStream&& other) noexcept;
  ~ZipEntryStream();

  static StatusOr<ZipEntryStream> Create(const std::string& path,
                                         const std::string& name,
                                         const ZipIndexEntry& entry);

  StatusOr<int> Read(char* buffer, unsigned int length);
  size_t size() const;

 private:
  ZipEntryStream(unzFile handle, size_t size, bool entry_open);

  Status OpenEntry(const std::string& name, const ZipIndexEntry& entry);
  void CloseEntry();

  unzFile handle_;
  size_t size_;
  bool entry_open_;
};

}

#endif
