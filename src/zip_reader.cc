// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#include "rockyou/pch.h"

#include "rockyou/zip_reader.h"

#include <utility>

namespace rockyou {
namespace {

class ZipFile {
 public:
  static StatusOr<ZipFile> Open(const std::string& path) {
    unzFile handle = unzOpen(path.c_str());
    if (handle == nullptr) {
      return Status::Error(std::string(kZipOpenError) + path);
    }
    return ZipFile(handle);
  }

  ZipFile(const ZipFile&) = delete;
  ZipFile& operator=(const ZipFile&) = delete;

  ZipFile(ZipFile&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  ZipFile& operator=(ZipFile&& other) noexcept {
    if (this != &other) {
      Close();
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~ZipFile() {
    Close();
  }

  unzFile get() const {
    return handle_;
  }

  unzFile Release() {
    unzFile temp = handle_;
    handle_ = nullptr;
    return temp;
  }

 private:
  explicit ZipFile(unzFile handle) : handle_(handle) {}

  void Close() {
    if (handle_ != nullptr) {
      unzClose(handle_);
      handle_ = nullptr;
    }
  }

  unzFile handle_;
};

}

StatusOr<ZipIndex> BuildZipIndex(const std::string& path) {
  auto zip_or = ZipFile::Open(path);
  if (!zip_or.ok()) {
    return zip_or.status();
  }
  ZipFile zip = zip_or.TakeValue();
  unz_global_info global_info;
  if (unzGetGlobalInfo(zip.get(), &global_info) != UNZ_OK) {
    return Status::Error(std::string(kZipInfoError));
  }
  if (global_info.number_entry == 0) {
    return ZipIndex();
  }
  if (unzGoToFirstFile(zip.get()) != UNZ_OK) {
    return Status::Error(std::string(kZipFirstFileError));
  }
  ZipIndex index;
  for (uLong i = 0; i < global_info.number_entry; ++i) {
    char filename_inzip[256];
    unz_file_info file_info;
    if (unzGetCurrentFileInfo(zip.get(), &file_info, filename_inzip,
                              sizeof(filename_inzip), nullptr, 0, nullptr, 0) != UNZ_OK) {
      return Status::Error(std::string(kZipFileInfoError));
    }
    uLong file_offset = unzGetOffset(zip.get());
    if (file_offset == 0) {
      std::println(stderr, kZipOffsetWarning, filename_inzip);
    }
    ZipIndexEntry entry;
    entry.offset = static_cast<size_t>(file_offset);
    entry.size = static_cast<size_t>(file_info.uncompressed_size);
    index[filename_inzip] = entry;
    if (i + 1 < global_info.number_entry) {
      if (unzGoToNextFile(zip.get()) != UNZ_OK) {
        return Status::Error(std::string(kZipNextFileError));
      }
    }
  }
  return ZipIndex(std::move(index));
}

ZipEntryStream::ZipEntryStream(unzFile handle, size_t size, bool entry_open)
    : handle_(handle), size_(size), entry_open_(entry_open) {}

ZipEntryStream::ZipEntryStream(ZipEntryStream&& other) noexcept
    : handle_(other.handle_), size_(other.size_), entry_open_(other.entry_open_) {
  other.handle_ = nullptr;
  other.size_ = 0;
  other.entry_open_ = false;
}

ZipEntryStream& ZipEntryStream::operator=(ZipEntryStream&& other) noexcept {
  if (this != &other) {
    CloseEntry();
    if (handle_ != nullptr) {
      unzClose(handle_);
    }
    handle_ = other.handle_;
    size_ = other.size_;
    entry_open_ = other.entry_open_;
    other.handle_ = nullptr;
    other.size_ = 0;
    other.entry_open_ = false;
  }
  return *this;
}

ZipEntryStream::~ZipEntryStream() {
  CloseEntry();
  if (handle_ != nullptr) {
    unzClose(handle_);
  }
}

StatusOr<ZipEntryStream> ZipEntryStream::Create(const std::string& path,
                                                const std::string& name,
                                                const ZipIndexEntry& entry) {
  auto zip_or = ZipFile::Open(path);
  if (!zip_or.ok()) {
    return zip_or.status();
  }
  ZipFile zip = zip_or.TakeValue();
  ZipEntryStream stream(zip.Release(), entry.size, false);
  Status status = stream.OpenEntry(name, entry);
  if (!status.ok()) {
    return status;
  }
  return std::move(stream);
}

Status ZipEntryStream::OpenEntry(const std::string& name, const ZipIndexEntry& entry) {
  if (handle_ == nullptr) {
    return Status::Error(std::string(kZipInvalidHandle));
  }
  if (entry.offset != 0) {
    if (unzSetOffset(handle_, static_cast<uLong>(entry.offset)) != UNZ_OK) {
      if (unzLocateFile(handle_, name.c_str(), 0) != UNZ_OK) {
        return Status::Error(std::string(kZipLocateError) + name);
      }
    }
  } else {
    if (unzLocateFile(handle_, name.c_str(), 0) != UNZ_OK) {
      return Status::Error(std::string(kZipLocateError) + name);
    }
  }
  if (unzOpenCurrentFile(handle_) != UNZ_OK) {
    return Status::Error(std::string(kZipOpenEntryError) + name);
  }
  entry_open_ = true;
  return Status::Ok();
}

void ZipEntryStream::CloseEntry() {
  if (entry_open_) {
    unzCloseCurrentFile(handle_);
    entry_open_ = false;
  }
}

StatusOr<int> ZipEntryStream::Read(char* buffer, unsigned int length) {
  if (!entry_open_) {
    return Status::Error(std::string(kZipClosedEntryReadError));
  }
  const int result = unzReadCurrentFile(handle_, buffer, length);
  if (result < 0) {
    return Status::Error(std::string(kZipReadError));
  }
  return result;
}

size_t ZipEntryStream::size() const {
  return size_;
}

}
