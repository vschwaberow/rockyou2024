// SPDX-License-Identifier: MIT
// (c) 2024 Volker Schwaberow <volker@schwaberow.de>

#ifndef ROCKYOU2024_INCLUDE_ROCKYOU_MESSAGES_H_
#define ROCKYOU2024_INCLUDE_ROCKYOU_MESSAGES_H_

#include <string_view>

namespace rockyou {

inline constexpr char kInteractiveFlag[] = "--interactive";
inline constexpr char kCaseInsensitiveFlag[] = "-i";
inline constexpr char kPromptEnterKeyword[] = "Enter the keyword to search: ";
inline constexpr char kPromptEnterZip[] = "Enter the zip filename to search in: ";
inline constexpr char kPromptCaseInsensitive[] = "Case-insensitive search? (y/n): ";
inline constexpr char kResponseYesLower[] = "y";
inline constexpr char kResponseYesUpper[] = "Y";
inline constexpr std::string_view kUsageHeader = "Usage: {} <zip_file> <keyword> [-i]";
inline constexpr std::string_view kUsageInteractive = "  or:  {} --interactive\n";
inline constexpr std::string_view kUsageOptionsHeader = "Options:";
inline constexpr std::string_view kUsageInteractiveOption = "  --interactive    Run in interactive mode";
inline constexpr std::string_view kUsageCaseInsensitiveOption = "  -i               Perform case-insensitive search";
inline constexpr std::string_view kUsageHelpOption = "  --help           Display this help message";
inline constexpr std::string_view kBannerForeground = "\033[1;34m";
inline constexpr std::string_view kBannerReset = "\033[0m";
inline constexpr std::string_view kBannerPrompt = "Press Enter to continue...";
inline constexpr std::string_view kFileMissingFormat = "File does not exist: {}\n";
inline constexpr std::string_view kErrorFormat = "{}\n";
inline constexpr std::string_view kKeywordEmptyError = "Keyword must not be empty";
inline constexpr std::string_view kNoEntriesMessage = "No entries found in zip file";
inline constexpr std::string_view kOccurrencesFormat = "Occurrences in \"{}\": {}";
inline constexpr std::string_view kOccurrenceDetailFormat = "  Line {}, Column {}: {}";
inline constexpr std::string_view kSearchCompleteFormat = "Search complete. Total occurrences: {}";
inline constexpr std::string_view kTimeTakenFormat = "Time taken: {} seconds";
inline constexpr std::string_view kZipOpenError = "Error opening zip file: ";
inline constexpr std::string_view kZipInfoError = "Error reading zip file info";
inline constexpr std::string_view kZipFirstFileError = "Error accessing first file in zip";
inline constexpr std::string_view kZipFileInfoError = "Error getting file info";
inline constexpr std::string_view kZipOffsetWarning = "Warning: Unable to get offset for file: {}";
inline constexpr std::string_view kZipNextFileError = "Error moving to next file in zip";
inline constexpr std::string_view kZipInvalidHandle = "Invalid zip handle";
inline constexpr std::string_view kZipLocateError = "Error locating file in zip: ";
inline constexpr std::string_view kZipOpenEntryError = "Error opening file in zip: ";
inline constexpr std::string_view kZipClosedEntryReadError = "Attempted to read from a closed zip entry";
inline constexpr std::string_view kZipReadError = "Error reading file content";
inline constexpr std::string_view kErrorProcessingFormat = "Error processing file \"{}\": {}";

}

#endif
