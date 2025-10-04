// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "rockyou/search_engine.h"
#include "rockyou/status.h"
#include "rockyou/zip_reader.h"

namespace {

std::filesystem::path TestDataPath(const std::string& name) {
  return std::filesystem::path(TEST_DATA_DIR) / name;
}

TEST(SearchEngineTest, BuildZipIndexEnumeratesEntries) {
  const auto index_status = rockyou::BuildZipIndex(TestDataPath("sample.zip").string());
  ASSERT_TRUE(index_status.ok()) << index_status.status().message();
  const auto& index = index_status.value();
  EXPECT_EQ(index.size(), 2u);
  EXPECT_NE(index.find("common.txt"), index.end());
  EXPECT_NE(index.find("nested/other.txt"), index.end());
}

TEST(SearchEngineTest, SearchZipFindsCaseSensitiveMatch) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  const rockyou::Status status = rockyou::SearchZip(TestDataPath("sample.zip").string(),
                                                    "password123",
                                                    options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_NE(output.find("Occurrences in \"common.txt\": 1"), std::string::npos);
  EXPECT_NE(output.find("Occurrences in \"nested/other.txt\": 0"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipFindsCaseInsensitiveMatches) {
  testing::internal::CaptureStdout();
  rockyou::SearchOptions options;
  options.case_insensitive = true;
  const rockyou::Status status = rockyou::SearchZip(TestDataPath("sample.zip").string(),
                                                    "password123",
                                                    options);
  const std::string output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_NE(output.find("Occurrences in \"common.txt\": 1"), std::string::npos);
  EXPECT_NE(output.find("Occurrences in \"nested/other.txt\": 1"), std::string::npos);
}

TEST(SearchEngineTest, SearchZipFailsOnMissingArchive) {
  const rockyou::Status status = rockyou::SearchZip(
      TestDataPath("missing.zip").string(), "anything", {});
  EXPECT_FALSE(status.ok());
}

}
