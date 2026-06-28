// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

import rockyou.search_engine;
import rockyou.error_handling;
import rockyou.zip_reader;
import rockyou.messages;

namespace rockyou {}

namespace {

std::filesystem::path TestDataPath(const std::string& name) { return std::filesystem::path(TEST_DATA_DIR) / name; }

TEST(SearchEngineTest, BuildZipIndexEnumeratesEntries) {
  const auto index_res = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_res.has_value()) << index_res.error().message;
  const auto& index = *index_res;
  EXPECT_EQ(index.size(), 2u);
  EXPECT_NE(index.find("common.txt"), index.end());
  EXPECT_NE(index.find("nested/other.txt"), index.end());
}

TEST(SearchEngineTest, SearchZipFindsCaseSensitiveMatch) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  auto status = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(status.has_value()) << status.error().message;
  EXPECT_NE(output.find("Occurrences in \"common.txt\": 1"), std::string::npos);
  EXPECT_NE(output.find("Occurrences in \"nested/other.txt\": 0"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipFindsCaseInsensitiveMatches) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.case_insensitive = true;
  auto status = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(status.has_value()) << status.error().message;
  EXPECT_NE(output.find("Occurrences in \"common.txt\": 1"), std::string::npos);
  EXPECT_NE(output.find("Occurrences in \"nested/other.txt\": 1"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipFailsOnMissingArchive) {
  const auto res = rockyou::SearchZip(TestDataPath("missing.zip").string(), "anything", {});
  EXPECT_FALSE(res.has_value());
}

TEST(SearchEngineTest, SearchZipRejectsEmptyKeyword) {
  rockyou::SearchOptions options;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::InvalidInput);
    EXPECT_EQ(res.error().message, std::string(rockyou::kKeywordEmptyError));
  }
}

TEST(SearchEngineTest, SearchZipRejectsNonPositiveLimit) {
  rockyou::SearchOptions options;
  options.limit = 0;
  auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::InvalidInput);
    EXPECT_EQ(res.error().message, std::string(rockyou::kInvalidLimitError));
  }

  options.limit = -5;
  res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
}

TEST(SearchEngineTest, SearchZipRejectsNonPositivePerFileLimit) {
  rockyou::SearchOptions options;
  options.per_file_limit = 0;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::InvalidInput);
    EXPECT_EQ(res.error().message, std::string(rockyou::kInvalidLimitError));
  }
}

TEST(SearchEngineTest, SearchZipRejectsZeroThreadCount) {
  rockyou::SearchOptions options;
  options.thread_count = 0;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::InvalidInput);
    EXPECT_EQ(res.error().message, std::string(rockyou::kInvalidThreadCountError));
  }
}

TEST(SearchEngineTest, SearchZipRejectsZeroChunkSize) {
  rockyou::SearchOptions options;
  options.chunk_size = 0;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::SearchError);
    EXPECT_EQ(res.error().message, std::string(rockyou::kInvalidChunkSizeError));
  }
}

TEST(SearchEngineTest, SearchZipRejectsZeroContextSize) {
  rockyou::SearchOptions options;
  options.context_size = 0;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::SearchError);
    EXPECT_EQ(res.error().message, std::string(rockyou::kInvalidContextSizeError));
  }
}

TEST(SearchEngineTest, SearchZipRejectsWrongChecksum) {
  rockyou::SearchOptions options;
  options.checksum = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::ChecksumMismatch);
    EXPECT_EQ(res.error().message, std::string(rockyou::kChecksumMismatchError));
  }
}

TEST(SearchEngineTest, SearchZipAcceptsCorrectSha256Checksum) {
  rockyou::SearchOptions options;
  // Precomputed SHA-256 of tests/data/sample.zip (see Phase 1 plan)
  options.checksum = "sha256:9440e92ae1760211ebc8b5a980c7637064920f3e500ab27a1914350e400c045e";
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  ASSERT_TRUE(res.has_value()) << (res ? "" : res.error().message);
}

TEST(SearchEngineTest, SearchZipJsonOutputContainsExpectedStructure) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.json = true;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value()) << (res ? "" : res.error().message);
  EXPECT_NE(output.find("\"total\":1"), std::string::npos);
  EXPECT_NE(output.find("\"file\":\"common.txt\""), std::string::npos);
  EXPECT_NE(output.find("\"case_insensitive\":false"), std::string::npos);
  EXPECT_NE(output.find("\"context\":"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipQuietModeProducesCompactLines) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.quiet = true;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value());
  EXPECT_NE(output.find("common.txt:2:6:"), std::string::npos);
  EXPECT_EQ(output.find("Occurrences in"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipHighlightInsertsBracketsInContext) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.highlight = true;
  options.context_size = 10;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value());
  EXPECT_NE(output.find("[password123]"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipContextSizeAffectsPrintedContext) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.context_size = 3;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value());
  EXPECT_NE(output.find("password123"), std::string::npos);
  EXPECT_EQ(output.find("alpha\nBeta password123 gamma"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipLimitCausesTruncationInJson) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.json = true;
  options.limit = 1;
  options.case_insensitive = true;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value());
  EXPECT_NE(output.find("\"truncated\":true"), std::string::npos);
  EXPECT_NE(output.find("\"limit\":1"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipRespectsExplicitThreadCount) {
  rockyou::SearchOptions options;
  options.thread_count = 2;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  ASSERT_TRUE(res.has_value()) << res.error().message;
}

TEST(SearchEngineTest, SearchZipRespectsHardwareFallbackThreadCount) {
  rockyou::SearchOptions options;
  options.thread_count = 1;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  ASSERT_TRUE(res.has_value()) << res.error().message;
}

TEST(SearchEngineTest, SearchZipDeterministicWithLimitAcrossThreads) {
  rockyou::SearchOptions options;
  options.json = true;
  options.case_insensitive = true;
  options.thread_count = 4;
  options.limit = 1;
  testing::internal::CaptureStdout();
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value()) << res.error().message;
  EXPECT_NE(output.find("\"truncated\":true"), std::string::npos);
  EXPECT_NE(output.find("\"total\":1"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipProducesFullCountWithoutLimit) {
  rockyou::SearchOptions options;
  options.json = true;
  options.case_insensitive = true;
  options.thread_count = 2;
  testing::internal::CaptureStdout();
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value()) << res.error().message;
  EXPECT_NE(output.find("\"total\":2"), std::string::npos);
  EXPECT_NE(output.find("\"truncated\":false"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipPerFileLimitAffectsResults) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.json = true;
  options.per_file_limit = 0;
  options.per_file_limit = 1;
  options.case_insensitive = true;
  const auto res = rockyou::SearchZip(TestDataPath("sample.zip").string(), "password123", options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(res.has_value());
  EXPECT_NE(output.find("\"per_file_limit\":1"), std::string::npos);
}

TEST(SearchEngineTest, ZipEntryStreamCreateAndReadFullContent) {
  const auto index_res = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_res.has_value());
  const auto& index = *index_res;

  auto entry_it = index.find("common.txt");
  ASSERT_NE(entry_it, index.end());

  auto stream_res =
      rockyou::ZipEntryStream::Create(TestDataPath("sample.zip").string(), "common.txt", entry_it->second);
  ASSERT_TRUE(stream_res.has_value()) << stream_res.error().message;

  rockyou::ZipEntryStream stream{std::move(stream_res.value())};
  EXPECT_EQ(stream.size(), 29u);

  std::string content(stream.size(), '\0');
  auto read_res = stream.Read(content.data(), static_cast<unsigned int>(content.size()));
  ASSERT_TRUE(read_res.has_value()) << (read_res ? "" : read_res.error().message);
  EXPECT_EQ(static_cast<size_t>(*read_res), content.size());

  EXPECT_EQ(content, "alpha\nBeta password123 gamma\n");
}

TEST(SearchEngineTest, ZipEntryStreamReadInChunks) {
  const auto index_res = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_res.has_value());
  auto entry_it = index_res->find("nested/other.txt");
  ASSERT_NE(entry_it, index_res->end());

  auto stream_res =
      rockyou::ZipEntryStream::Create(TestDataPath("sample.zip").string(), "nested/other.txt", entry_it->second);
  ASSERT_TRUE(stream_res.has_value());

  rockyou::ZipEntryStream stream{std::move(stream_res.value())};

  std::string content;
  char buf[8];
  while (true) {
    auto read_res = stream.Read(buf, sizeof(buf));
    if (!read_res.has_value() || *read_res <= 0)
      break;
    content.append(buf, static_cast<size_t>(*read_res));
  }
  EXPECT_EQ(content, "delta\nPASSWORD123 epsilon\n");
}

TEST(SearchEngineTest, ZipEntryStreamCloseWithStatusSuccess) {
  const auto index_res = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_res.has_value());
  auto entry_it = index_res->find("common.txt");
  ASSERT_NE(entry_it, index_res->end());

  auto stream_res =
      rockyou::ZipEntryStream::Create(TestDataPath("sample.zip").string(), "common.txt", entry_it->second);
  ASSERT_TRUE(stream_res.has_value());

  rockyou::ZipEntryStream stream{std::move(stream_res.value())};
  const auto close_res = stream.CloseWithStatus();
  ASSERT_TRUE(close_res.has_value()) << close_res.error().message;
}

TEST(SearchEngineTest, ZipEntryStreamReadAfterCloseFailsWithSpecificError) {
  const auto index_res = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_res.has_value());
  auto entry_it = index_res->find("common.txt");
  ASSERT_NE(entry_it, index_res->end());

  auto stream_res =
      rockyou::ZipEntryStream::Create(TestDataPath("sample.zip").string(), "common.txt", entry_it->second);
  ASSERT_TRUE(stream_res.has_value());

  rockyou::ZipEntryStream stream{std::move(stream_res.value())};
  auto close_res = stream.CloseWithStatus();
  ASSERT_TRUE(close_res.has_value());

  char buf[10];
  const auto read_res = stream.Read(buf, sizeof(buf));
  EXPECT_FALSE(read_res.has_value());
  if (!read_res) {
    EXPECT_EQ(read_res.error().code, rockyou::ErrorCode::ZipError);
    EXPECT_EQ(read_res.error().message, std::string(rockyou::kZipClosedEntryReadError));
  }
}

TEST(SearchEngineTest, ZipEntryStreamCreateOnNonExistentEntryFails) {
  const auto index_res = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_res.has_value());

  rockyou::ZipIndexEntry bad_entry{0, 10};
  auto stream_res =
      rockyou::ZipEntryStream::Create(TestDataPath("sample.zip").string(), "does_not_exist.txt", bad_entry);

  EXPECT_FALSE(stream_res.has_value());
  if (!stream_res) {
    EXPECT_EQ(stream_res.error().code, rockyou::ErrorCode::ZipError);
  }
}

TEST(SearchEngineTest, BuildZipIndexOnMissingFileReturnsError) {
  const auto res = rockyou::BuildZipIndex(TestDataPath("nonexistent.zip").string());
  EXPECT_FALSE(res.has_value());
  if (!res) {
    EXPECT_EQ(res.error().code, rockyou::ErrorCode::ZipError);
  }
}

std::string GetSearchBinary() {
  std::filesystem::path root = std::filesystem::path(TEST_DATA_DIR).parent_path().parent_path();
  return (root / "build" / "debug" / "bin" / "search").string();
}

TEST(SearchEngineTest, CLIHelpReturnsSuccess) {
  std::string bin = GetSearchBinary();
  std::string cmd = bin + " --help > /tmp/rockyou_cli_help.txt 2>&1";
  int ret = std::system(cmd.c_str());
  EXPECT_EQ(ret, 0);
}

TEST(SearchEngineTest, CLIBasicSearchReturnsSuccessAndProducesOutput) {
  std::string bin = GetSearchBinary();
  std::string zip = TestDataPath("sample.zip").string();
  std::string cmd = bin + " " + zip + " password123 > /tmp/rockyou_cli_basic.txt 2>&1";
  int ret = std::system(cmd.c_str());
  EXPECT_EQ(ret, 0);

  std::ifstream f("/tmp/rockyou_cli_basic.txt");
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("Occurrences in \"common.txt\""), std::string::npos);
}

TEST(SearchEngineTest, CLIJsonFlagProducesJsonOutput) {
  std::string bin = GetSearchBinary();
  std::string zip = TestDataPath("sample.zip").string();
  std::string cmd = bin + " " + zip + " password123 --json > /tmp/rockyou_cli_json.txt 2>&1";
  int ret = std::system(cmd.c_str());
  EXPECT_EQ(ret, 0);

  std::ifstream f("/tmp/rockyou_cli_json.txt");
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("\"total\""), std::string::npos);
  EXPECT_NE(content.find("\"file\":\"common.txt\""), std::string::npos);
}

TEST(SearchEngineTest, CLIErrorOnMissingFileReturnsNonZero) {
  std::string bin = GetSearchBinary();
  std::string cmd = bin + " /nonexistent/file.zip foo > /tmp/rockyou_cli_err.txt 2>&1";
  int ret = std::system(cmd.c_str());
  EXPECT_NE(ret, 0);
}

TEST(SearchEngineTest, CLILongKeywordDoesNotCrash) {
  std::string bin = GetSearchBinary();
  std::string long_kw(1000, 'a');
  std::string zip = TestDataPath("sample.zip").string();
  std::string cmd = bin + " " + zip + " " + long_kw + " > /tmp/rockyou_cli_long.txt 2>&1";
  int ret = std::system(cmd.c_str());
  EXPECT_GE(ret, 0);
}

} // namespace
