#include <algorithm>
#include <chrono>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <atomic>
#include <ranges>
#include <span>
#include "unzip.h"
#include "zlib.h"

constexpr size_t CHUNK_SIZE = 1024 * 1024;                  // 1 MB
constexpr size_t MIN_FILE_SIZE_FOR_MMAP = 10 * 1024 * 1024; // 10 MB

struct FileInfo
{
    size_t offset;
    size_t size;
};

struct SearchResult
{
    std::string filename;
    std::vector<std::pair<int, int>> occurrences;
};

using ZipIndex = std::map<std::string, FileInfo>;

void print_header()
{
    std::string_view ascii_art = R"(
 ____   ___   ____ _  ____   __ ___  _   _ ____   ___ ____  _  _
|  _ \ / _ \ / ___| |/ /\ \ / // _ \| | | |___ \ / _ \___ \| || |
| |_) | | | | |   | ' /  \ V /| | | | | | | __) | | | |__) | || |_
|  _ <| |_| | |___| . \   | | | |_| | |_| |/ __/| |_| / __/|__   _|
|_| \_\\___/ \____|_|\_\  |_|  \___/ \___/|_____|\___/_____|  |_|

© 2024 Volker Schwaberow <volker@schwaberow.de>
Based on rockyou2024 cpp by Mike Madden

)";

    std::cout << "\033[1;34m"; // Set text color to bright blue

    for (char c : ascii_art)
    {
        std::cout << c << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    std::cout << "\033[0m";
    std::cout << "Press Enter to continue...";
    std::cin.get();
}

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <zip_file> <keyword>\n"
              << "  or:  " << program_name << " --interactive\n\n"
              << "Options:\n"
              << "  --interactive    Run in interactive mode\n"
              << "  --help           Display this help message\n";
}

ZipIndex create_zip_index(const std::string &filename)
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
        if (unzGetCurrentFileInfo(zip_file, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0) != UNZ_OK)
        {
            unzClose(zip_file);
            throw std::runtime_error("Error getting file info");
        }

        index[filename_inzip] = {unzGetOffset(zip_file), file_info.uncompressed_size};

        if (i + 1 < global_info.number_entry && unzGoToNextFile(zip_file) != UNZ_OK)
        {
            unzClose(zip_file);
            throw std::runtime_error("Error moving to next file in zip");
        }
    }

    unzClose(zip_file);
    return index;
}

std::vector<size_t> boyer_moore(std::string_view text, std::string_view pattern)
{
    std::vector<size_t> results;
    int m = pattern.length();
    int n = text.length();

    if (m == 0 || n == 0 || m > n)
        return results;

    std::array<int, 256> badChar;
    badChar.fill(-1);

    for (int i = 0; i < m; i++)
        badChar[pattern[i]] = i;

    for (int s = 0; s <= (n - m); s += std::max(1, [&]()
                                                {
        for (int j = m - 1; j >= 0; --j)
        {
            if (pattern[j] != text[s + j])
                return j - badChar[text[s + j]];
        }
        results.push_back(s);
        return m - ((s + m < n) ? badChar[text[s + m]] : -1); }()))
        ;

    return results;
}

SearchResult search_in_file(const std::string &zip_filename, const FileInfo &file_info, const std::string &keyword)
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

    if (unzSetOffset(zip_file, file_info.offset) != UNZ_OK || unzOpenCurrentFile(zip_file) != UNZ_OK)
    {
        cleanup();
        throw std::runtime_error("Error setting offset or opening file in zip");
    }

    SearchResult result;
    result.filename = zip_filename;

    if (file_info.size >= MIN_FILE_SIZE_FOR_MMAP)
    {
        std::vector<char> buffer(file_info.size);
        if (unzReadCurrentFile(zip_file, buffer.data(), static_cast<unsigned int>(buffer.size())) != static_cast<int>(file_info.size))
        {
            cleanup();
            throw std::runtime_error("Error reading file content");
        }
        std::string_view content(buffer.data(), buffer.size());
        auto positions = boyer_moore(content, keyword);

        int line = 1, col = 1;
        for (size_t pos : positions)
        {
            while (pos > 0 && content[pos - 1] != '\n')
            {
                pos--;
                col++;
            }
            result.occurrences.emplace_back(line, col);
            line++;
            col = 1;
        }
    }
    else
    {
        std::vector<char> buffer(CHUNK_SIZE);
        std::string overlap;
        int bytes_read;
        size_t total_read = 0;
        int line = 1, col = 1;
        while ((bytes_read = unzReadCurrentFile(zip_file, buffer.data(), static_cast<unsigned int>(buffer.size()))) > 0)
        {
            std::string_view chunk(buffer.data(), static_cast<size_t>(bytes_read));
            std::string search_text = overlap + std::string(chunk);

            auto positions = boyer_moore(search_text, keyword);
            for (size_t pos : positions)
            {
                size_t line_start = total_read + pos;
                while (line_start > 0 && search_text[line_start - 1] != '\n')
                {
                    line_start--;
                }
                result.occurrences.emplace_back(line, pos - line_start + 1);
                line++;
            }

            for (char c : chunk)
            {
                if (c == '\n')
                {
                    line++;
                    col = 1;
                }
                else
                {
                    col++;
                }
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

void search_in_zip(const std::string &filename, const std::string &keyword)
{
    auto index = create_zip_index(filename);

    const auto start_time = std::chrono::high_resolution_clock::now();

    std::atomic<int> total_count = 0;
    std::mutex cout_mutex;

    std::vector<std::thread> threads;
    std::vector<SearchResult> results(index.size());
    int i = 0;
    for (const auto &[file_name, file_info] : index)
    {
        threads.emplace_back([&, i](const std::string &fn, const FileInfo &fi)
                             {
            results[i] = search_in_file(filename, fi, keyword);
            total_count += results[i].occurrences.size();

            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Occurrences in \"" << fn << "\": " << results[i].occurrences.size() << '\n';
            for (const auto &[line, col] : results[i].occurrences) {
                std::cout << "  Line " << line << ", Column " << col << '\n';
            } }, file_name, file_info);
        i++;
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

int main(int argc, char *argv[])
{
    try
    {
        print_header();

        std::string keyword;
        std::string filename;

        if (argc == 2 && (std::strcmp(argv[1], "--help") == 0))
        {
            print_usage(argv[0]);
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
            print_usage(argv[0]);
            return 1;
        }

        if (!std::filesystem::exists(filename))
        {
            throw std::runtime_error("File does not exist: " + filename);
        }

        search_in_zip(filename, keyword);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}