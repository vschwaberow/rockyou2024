#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "unzip.h"
#include "zlib.h"

namespace Search
{

    constexpr size_t kChunkSize = 1024 * 1024;               // 1 MB
    constexpr size_t kMinFileSizeForMmap = 10 * 1024 * 1024; // 10 MB
    constexpr int kContextSize = 20;

    struct FileInfo
    {
        size_t offset;
        size_t size;
    };

    struct SearchResult
    {
        std::string filename;
        std::vector<std::tuple<int, int, std::string>> occurrences;
    };

    using ZipIndex = std::map<std::string, FileInfo>;

    void PrintHeader()
    {
        const char *kAsciiArt = R"(
 ____   ___   ____ _  ____   __ ___  _   _ ____   ___ ____  _  _
|  _ \ / _ \ / ___| |/ /\ \ / // _ \| | | |___ \ / _ \___ \| || |
| |_) | | | | |   | ' /  \ V /| | | | | | | __) | | | |__) | || |_
|  _ <| |_| | |___| . \   | | | |_| | |_| |/ __/| |_| / __/|__   _|
|_| \_\\___/ \____|_|\_\  |_|  \___/ \___/|_____|\___/_____|  |_|

© 2024 Volker Schwaberow <volker@schwaberow.de>
Based on rockyou2024 cpp by Mike Madden

)";

        std::cout << "\033[1;34m"; // Set text color to bright blue
        for (size_t i = 0; kAsciiArt[i] != '\0'; ++i)
        {
            std::cout << kAsciiArt[i] << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        std::cout << "\033[0m";
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }

    void PrintUsage(const char *program_name)
    {
        std::cout << "Usage: " << program_name << " <zip_file> <keyword> [-i]\n"
                  << "  or:  " << program_name << " --interactive\n\n"
                  << "Options:\n"
                  << "  --interactive    Run in interactive mode\n"
                  << "  -i               Perform case-insensitive search\n"
                  << "  --help           Display this help message\n";
    }

    ZipIndex CreateZipIndex(const std::string &filename)
    {
        ZipIndex index;
        unzFile zip_file = unzOpen(filename.c_str());
        if (!zip_file)
        {
            throw std::runtime_error("Error opening zip file: " + filename);
        }

        unz_global_info global_info;
        if (unzGetGlobalInfo(zip_file, &global_info) != UNZ_OK)
        {
            unzClose(zip_file);
            throw std::runtime_error("Error reading zip file info");
        }

        for (uLong i = 0; i < global_info.number_entry; ++i)
        {
            char filename_inzip[256];
            unz_file_info file_info;
            if (unzGetCurrentFileInfo(zip_file, &file_info, filename_inzip,
                                      sizeof(filename_inzip), nullptr, 0, nullptr,
                                      0) != UNZ_OK)
            {
                unzClose(zip_file);
                throw std::runtime_error("Error getting file info");
            }

            uLong file_offset = unzGetOffset(zip_file);
            if (file_offset == 0)
            {
                std::cerr << "Warning: Unable to get offset for file: " << filename_inzip
                          << std::endl;
            }

            index[filename_inzip] = {file_offset, file_info.uncompressed_size};

            if (i + 1 < global_info.number_entry && unzGoToNextFile(zip_file) != UNZ_OK)
            {
                unzClose(zip_file);
                throw std::runtime_error("Error moving to next file in zip");
            }
        }

        unzClose(zip_file);
        return index;
    }

    std::vector<size_t> BoyerMoore(std::string_view text, std::string_view pattern)
    {
        std::vector<size_t> results;
        int m = pattern.length();
        int n = text.length();

        if (m == 0 || n == 0 || m > n)
            return results;

        std::array<int, 256> bad_char;
        bad_char.fill(-1);

        for (int i = 0; i < m; i++)
            bad_char[pattern[i]] = i;

        for (int s = 0; s <= (n - m);)
        {
            int j = m - 1;
            while (j >= 0 && pattern[j] == text[s + j])
                j--;

            if (j < 0)
            {
                results.push_back(s);
                s += (s + m < n) ? m - bad_char[text[s + m]] : 1;
            }
            else
            {
                s += std::max(1, j - bad_char[text[s + j]]);
            }
        }

        return results;
    }

    SearchResult SearchInFile(const std::string &zip_filename,
                              const std::string &file_name,
                              const FileInfo &file_info,
                              const std::string &keyword)
    {
        unzFile zip_file = unzOpen(zip_filename.c_str());
        if (!zip_file)
        {
            throw std::runtime_error("Error opening zip file: " + zip_filename);
        }

        auto cleanup = [&]()
        {
            unzCloseCurrentFile(zip_file);
            unzClose(zip_file);
        };

        if (file_info.offset != 0 && unzSetOffset(zip_file, file_info.offset) != UNZ_OK)
        {
            std::cerr << "Warning: Unable to set offset for file: " << file_name
                      << ". Trying to locate file by name." << std::endl;
            if (unzLocateFile(zip_file, file_name.c_str(), 0) != UNZ_OK)
            {
                cleanup();
                throw std::runtime_error("Error locating file in zip: " + file_name);
            }
        }

        if (unzOpenCurrentFile(zip_file) != UNZ_OK)
        {
            cleanup();
            throw std::runtime_error("Error opening file in zip: " + file_name);
        }

        SearchResult result;
        result.filename = file_name;

        if (file_info.size >= kMinFileSizeForMmap)
        {
            std::vector<char> buffer(file_info.size);
            if (unzReadCurrentFile(zip_file, buffer.data(),
                                   static_cast<unsigned int>(buffer.size())) !=
                static_cast<int>(file_info.size))
            {
                cleanup();
                throw std::runtime_error("Error reading file content");
            }
            std::string_view content(buffer.data(), buffer.size());
            auto positions = BoyerMoore(content, keyword);

            int line = 1, col = 1;
            for (size_t pos : positions)
            {
                size_t line_start = pos;
                while (line_start > 0 && content[line_start - 1] != '\n')
                {
                    line_start--;
                    col++;
                }

                size_t context_start = (pos > kContextSize) ? pos - kContextSize : 0;
                size_t context_end = std::min(pos + keyword.length() + kContextSize, content.length());
                std::string context(content.substr(context_start, context_end - context_start));

                result.occurrences.emplace_back(line, col, std::move(context));

                line += std::count(content.begin() + line_start, content.begin() + pos, '\n');
                col = pos - line_start + 1;
            }
        }
        else
        {
            std::vector<char> buffer(kChunkSize);
            std::string overlap;
            int bytes_read;
            size_t total_read = 0;
            int line = 1, col = 1;
            while ((bytes_read = unzReadCurrentFile(zip_file, buffer.data(),
                                                    static_cast<unsigned int>(buffer.size()))) > 0)
            {
                std::string_view chunk(buffer.data(), static_cast<size_t>(bytes_read));
                std::string search_text = overlap + std::string(chunk);

                auto positions = BoyerMoore(search_text, keyword);
                for (size_t pos : positions)
                {
                    size_t line_start = pos;
                    while (line_start > 0 && search_text[line_start - 1] != '\n')
                    {
                        line_start--;
                    }

                    size_t context_start = (pos > kContextSize) ? pos - kContextSize : 0;
                    size_t context_end = std::min(pos + keyword.length() + kContextSize, search_text.length());
                    std::string context = search_text.substr(context_start, context_end - context_start);

                    result.occurrences.emplace_back(line, pos - line_start + 1, std::move(context));

                    line += std::count(search_text.begin() + line_start, search_text.begin() + pos, '\n');
                }

                total_read += bytes_read;
                overlap = (search_text.length() >= keyword.length())
                              ? search_text.substr(search_text.length() - keyword.length() + 1)
                              : "";
            }
        }

        cleanup();
        return result;
    }

    void SearchInZip(const std::string &filename, const std::string &keyword)
    {
        auto index = CreateZipIndex(filename);

        const auto start_time = std::chrono::high_resolution_clock::now();

        std::atomic<int> total_count = 0;
        std::mutex cout_mutex;

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0)
        {
            num_threads = 4; // Default to 4 threads if hardware_concurrency() fails
        }

        std::vector<std::thread> threads;
        std::vector<SearchResult> results;
        results.reserve(index.size());

        std::atomic<size_t> next_file_index(0);

        auto worker = [&]()
        {
            while (true)
            {
                size_t i = next_file_index.fetch_add(1, std::memory_order_relaxed);
                if (i >= index.size())
                    break;

                auto it = std::next(index.begin(), i);
                const auto &[file_name, file_info] = *it;

                try
                {
                    SearchResult result = SearchInFile(filename, file_name, file_info, keyword);
                    total_count += result.occurrences.size();

                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Occurrences in \"" << file_name << "\": " << result.occurrences.size() << '\n';
                    for (const auto &[line, col, context] : result.occurrences)
                    {
                        std::cout << "  Line " << line << ", Column " << col << ": " << context << '\n';
                    }

                    results.push_back(std::move(result));
                }
                catch (const std::exception &e)
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cerr << "Error processing file \"" << file_name << "\": " << e.what() << '\n';
                }
            }
        };

        for (unsigned int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back(worker);
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> cpu_time_used = end_time - start_time;

        std::cout << "Search complete. Total occurrences: " << total_count << '\n';
        std::cout << "Time taken: " << cpu_time_used.count() << " seconds\n";
    }
} 

int main(int argc, char *argv[])
{
    try
    {
        Search::PrintHeader();

        std::string keyword;
        std::string filename;

        if (argc == 2 && (std::strcmp(argv[1], "--help") == 0))
        {
            Search::PrintUsage(argv[0]);
            return 0;
        }

        if (argc == 2 && (std::strcmp(argv[1], "--interactive") == 0))
        {
            std::cout << "Enter the keyword to search: ";
            std::getline(std::cin, keyword);

            std::cout << "Enter the zip filename to search in: ";
            std::getline(std::cin, filename);
        }
        else if (argc == 3)
        {
            filename = argv[1];
            keyword = argv[2];
        }
        else
        {
            Search::PrintUsage(argv[0]);
            return 1;
        }

        if (!std::filesystem::exists(filename))
        {
            throw std::runtime_error("File does not exist: " + filename);
        }

        Search::SearchInZip(filename, keyword);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}