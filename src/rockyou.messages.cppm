// SPDX-License-Identifier: MIT
// (c) 2024-2026 Volker Schwaberow <volker@schwaberow.de>

module;

#include <string_view>

export module rockyou.messages;

export namespace rockyou {

inline constexpr std::string_view kInteractiveFlag = "--interactive";
inline constexpr std::string_view kCaseInsensitiveFlag = "-i";
inline constexpr std::string_view kQuietFlag = "--quiet";
inline constexpr std::string_view kJsonFlag = "--json";
inline constexpr std::string_view kCountFlag = "--count";
inline constexpr std::string_view kStatsFlag = "--stats";
inline constexpr std::string_view kLimitFlag = "--limit";
inline constexpr std::string_view kPerFileLimitFlag = "--per-file-limit";
inline constexpr std::string_view kThreadsFlag = "--threads";
inline constexpr std::string_view kChunkSizeFlag = "--chunk";
inline constexpr std::string_view kContextSizeFlag = "--context";
inline constexpr std::string_view kChecksumFlag = "--checksum";
inline constexpr std::string_view kHighlightFlag = "--highlight";
inline constexpr std::string_view kPromptEnterKeyword = "Enter the keyword to search: ";
inline constexpr std::string_view kPromptEnterZip = "Enter the zip filename to search in: ";
inline constexpr std::string_view kPromptCaseInsensitive = "Case-insensitive search? (y/n): ";
inline constexpr std::string_view kPromptAnotherSearch = "Run another search? (y/n): ";
inline constexpr std::string_view kPromptEnterLimit = "Enter a positive limit (or leave blank for no limit): ";
inline constexpr std::string_view kPromptEnterThreads = "Enter thread count (or leave blank for default): ";
inline constexpr std::string_view kPromptEnterChunk = "Enter chunk size in bytes (or leave blank for default): ";
inline constexpr std::string_view kPromptEnterContext = "Enter context length in chars (or leave blank for default): ";
inline constexpr std::string_view kPromptEnterChecksum =
    "Enter expected archive checksum (or leave blank to skip integrity check): ";
inline constexpr std::string_view kResponseYesLower = "y";
inline constexpr std::string_view kResponseYesUpper = "Y";
inline constexpr std::string_view kUsageHeader = "Usage: {} <zip_file> <keyword> [-i]";
inline constexpr std::string_view kUsageInteractive = "  or:  {} --interactive\n";
inline constexpr std::string_view kUsageOptionsHeader = "Options:";
inline constexpr std::string_view kUsageInteractiveOption = "  --interactive    Run in interactive mode";
inline constexpr std::string_view kUsageCaseInsensitiveOption = "  -i               Perform case-insensitive search";
inline constexpr std::string_view kUsageQuietOption = "  --quiet          Suppress headers/timing; emit matches only";
inline constexpr std::string_view kUsageJsonOption = "  --json           Emit results as JSON with ordering";
inline constexpr std::string_view kUsageCountOption = "  --count          Print only the total match count";
inline constexpr std::string_view kUsageStatsOption = "  --stats          Print search statistics to stderr";
inline constexpr std::string_view kUsageLimitOption =
    "  --limit N        Cap total matches to N (indicates truncation)";
inline constexpr std::string_view kUsagePerFileLimitOption =
    "  --per-file-limit N Cap matches per file to N (indicates truncation)";
inline constexpr std::string_view kUsageThreadsOption = "  --threads N      Force thread count to N (>=1)";
inline constexpr std::string_view kUsageChunkOption = "  --chunk BYTES    Set chunk size in bytes (>=1)";
inline constexpr std::string_view kUsageContextOption = "  --context CHARS  Set context length in characters (>=1)";
inline constexpr std::string_view kUsageChecksumOption =
    "  --checksum HEX   Expected checksum to verify archive before search";
inline constexpr std::string_view kUsageHighlightOption = "  --highlight      Mark matched text in context output";
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
inline constexpr std::string_view kInvalidLimitError = "Limit must be greater than zero";
inline constexpr std::string_view kInvalidThreadCountError = "Thread count must be at least one";
inline constexpr std::string_view kInvalidChunkSizeError = "Chunk size must be greater than zero";
inline constexpr std::string_view kInvalidContextSizeError = "Context length must be greater than zero";
inline constexpr std::string_view kChecksumMismatchError = "Archive checksum mismatch; aborting search";
inline constexpr std::string_view kCorruptEntryWarning = "Warning: Skipping corrupt entry \"{}\": {}";
inline constexpr std::string_view kPartialFailureWarning =
    "Search completed with errors. Some entries could not be processed.";
inline constexpr std::string_view kJsonSerializationError = "Failed to serialize results to JSON";
inline constexpr std::string_view kInteractiveInvalidInput = "Invalid input. Please try again.";
inline constexpr std::string_view kResultsTruncatedMessage = "Results truncated at configured limit.";
inline constexpr std::string_view kQuietJsonConflictError = "Cannot combine --quiet and --json options.";
inline constexpr std::string_view kInteractiveHelp =
    "Commands: [s]earch, [q]uit. Provide zip, keyword, optional flags.";
inline constexpr std::string_view kInteractivePromptAction = "Action (s=search, q=quit): ";
inline constexpr std::string_view kInteractiveExit = "Exiting interactive mode.";

inline constexpr std::string_view kRegexFlag = "--regex";
inline constexpr std::string_view kRegexModeFlag = "--regex-mode";
inline constexpr std::string_view kUsageRegexOption =
    "  --regex          Enable regex pattern matching instead of literal keyword";
inline constexpr std::string_view kUsageRegexModeOption =
    "  --regex-mode MODE Set regex mode: ecmascript|awk|grep|egrep (default: ecmascript)";
inline constexpr std::string_view kErrorEmptyRegexPattern = "Regex pattern must not be empty";
inline constexpr std::string_view kErrorInvalidRegexPattern = "Invalid regex pattern '{}': {}";
inline constexpr std::string_view kRegexPatternFormat = "Regex pattern: {} ({})";
inline constexpr std::string_view kPromptUseRegex = "Use regex pattern matching? (y/n): ";
inline constexpr std::string_view kPromptEnterRegexPattern = "Enter the regex pattern to search: ";
inline constexpr std::string_view kPromptEnterRegexMode =
    "Enter regex mode (ecmascript/awk/grep/egrep, or leave blank for default): ";

inline constexpr std::string_view kProjectName = "rockyou2024";
inline constexpr std::string_view kProjectVersion = "0.7.0";
inline constexpr std::string_view kProjectAuthor = "Volker Schwaberow <volker@schwaberow.de>";
inline constexpr std::string_view kProjectLicense = "MIT";

inline constexpr unsigned int kDefaultThreadCountFallback = 4;
inline constexpr size_t kWorkBatchSize = 8;

} // namespace rockyou
