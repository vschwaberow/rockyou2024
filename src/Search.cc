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
#include <queue>
#include <unordered_map>
#include <memory>
#include <semaphore>
#include <functional>
#include <ranges>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "unzip.h"
#include "zlib.h"

namespace Search
{

    bool case_insensitive = false;
    constexpr size_t kChunkSize = 1024 * 1024;
    constexpr size_t kMinFileSizeForMmap = 10 * 1024 * 1024;
    constexpr int kContextSize = 20;

    struct FileInfo
    {
        size_t offset;
        size_t size;
    };

    struct SearchResult
    {
        std::string filename;
        std::vector<std::tuple<std::string, int, int, std::string>> occurrences;
    };

    using ZipIndex = std::map<std::string, FileInfo>;

    class ThreadPool
    {
    public:
        ThreadPool(size_t num_threads) : stop_flag(false)
        {
            for (size_t i = 0; i < num_threads; ++i)
            {
                threads.emplace_back([this]
                                     { worker_thread(); });
            }
        }

        ~ThreadPool()
        {
            {
                std::lock_guard lock(queue_mutex);
                stop_flag = true;
            }
            sem.release(threads.size());
            for (auto &thread : threads)
            {
                thread.join();
            }
        }

        template <class F>
        void enqueue(F &&f)
        {
            {
                std::lock_guard lock(queue_mutex);
                tasks.emplace(std::forward<F>(f));
            }
            sem.release();
        }

    private:
        void worker_thread()
        {
            while (true)
            {
                sem.acquire();
                std::function<void()> task;
                {
                    std::lock_guard lock(queue_mutex);
                    if (stop_flag && tasks.empty())
                    {
                        return;
                    }
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        }

        std::vector<std::jthread> threads;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::binary_semaphore sem{0};
        bool stop_flag;
    };

    class AhoCorasick
    {
    private:
        struct Node
        {
            std::unordered_map<char, std::unique_ptr<Node>> children;
            Node *fail;
            std::vector<int> output;
            Node() : fail(nullptr) {}
        };

        std::unique_ptr<Node> root;

    public:
        AhoCorasick(const std::vector<std::string> &patterns)
        {
            root = std::make_unique<Node>();

            for (int i = 0; i < patterns.size(); ++i)
            {
                Node *node = root.get();
                for (char ch : patterns[i])
                {
                    if (!node->children[ch])
                    {
                        node->children[ch] = std::make_unique<Node>();
                    }
                    node = node->children[ch].get();
                }
                node->output.push_back(i);
            }

            buildFailureLinks();
        }

        std::vector<std::pair<int, int>> search(const std::string &text)
        {
            std::vector<std::pair<int, int>> results;
            Node *current = root.get();

            for (int i = 0; i < text.length(); ++i)
            {
                char ch = text[i];
                while (current != root.get() && !current->children[ch])
                {
                    current = current->fail;
                }

                if (current->children[ch])
                {
                    current = current->children[ch].get();
                }

                for (int pattern_index : current->output)
                {
                    results.emplace_back(pattern_index, i);
                }
            }

            return results;
        }

    private:
        void buildFailureLinks()
        {
            std::queue<Node *> q;

            for (auto &[ch, child] : root->children)
            {
                child->fail = root.get();
                q.push(child.get());
            }

            while (!q.empty())
            {
                Node *node = q.front();
                q.pop();

                for (auto &[ch, child] : node->children)
                {
                    q.push(child.get());

                    Node *fail = node->fail;
                    while (fail != root.get() && !fail->children[ch])
                    {
                        fail = fail->fail;
                    }

                    child->fail = fail->children[ch] ? fail->children[ch].get() : root.get();
                    child->output.insert(child->output.end(),
                                         child->fail->output.begin(),
                                         child->fail->output.end());
                }
            }
        }
    };

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

        std::cout << "\033[1;34m";
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
        std::cout << "Usage: " << program_name << " <zip_file> <keyword1> [<keyword2> ...] [-i]\n"
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

    SearchResult SearchInFile(const std::string &zip_filename,
                              const std::string &file_name,
                              const FileInfo &file_info,
                              const std::vector<std::string> &keywords)
    {
        int fd = open(zip_filename.c_str(), O_RDONLY);
        if (fd == -1)
        {
            throw std::runtime_error("Error opening zip file for mmap");
        }

        void *mapped = mmap(nullptr, file_info.size, PROT_READ, MAP_PRIVATE, fd, file_info.offset);
        if (mapped == MAP_FAILED)
        {
            close(fd);
            throw std::runtime_error("Error memory mapping file");
        }

        SearchResult result;
        result.filename = file_name;

        AhoCorasick ac(keywords);

        std::string_view content(static_cast<char *>(mapped), file_info.size);
        auto matches = ac.search(std::string(content));

        for (const auto &[keyword_index, pos] : matches)
        {
            const auto &keyword = keywords[keyword_index];
            int line = 1, col = 1;
            for (size_t i = 0; i < pos; ++i)
            {
                if (content[i] == '\n')
                {
                    ++line;
                    col = 1;
                }
                else
                {
                    ++col;
                }
            }

            size_t context_start = (pos > kContextSize) ? pos - kContextSize : 0;
            size_t context_end = std::min(pos + keyword.length() + kContextSize, content.length());
            std::string context(content.substr(context_start, context_end - context_start));

            result.occurrences.emplace_back(keyword, line, col, std::move(context));
        }

        munmap(mapped, file_info.size);
        close(fd);

        return result;
    }

    void SearchInZip(const std::string &filename, const std::vector<std::string> &keywords)
    {
        ZipIndex index = CreateZipIndex(filename);
        std::vector<SearchResult> results;
        std::mutex cout_mutex;
        size_t total_count = 0;
        std::atomic_ref<size_t> atomic_total_count(total_count);

        ThreadPool pool(std::thread::hardware_concurrency());

        for (const auto &[file_name, file_info] : index)
        {
            pool.enqueue([&, file_name, file_info]
                         {
                try {
                    SearchResult result = SearchInFile(filename, file_name, file_info, keywords);
                    atomic_total_count.fetch_add(result.occurrences.size(), std::memory_order_relaxed);

                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Occurrences in \"" << file_name << "\":\n";
                    for (const auto& [keyword, line, col, context] : result.occurrences) {
                        std::cout << "  Keyword \"" << keyword << "\" at Line " << line << ", Column " << col << ": " << context << '\n';
                    }

                    results.push_back(std::move(result));
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cerr << "Error processing file \"" << file_name << "\": " << e.what() << '\n';
                } });
        }

        pool.~ThreadPool();

        std::cout << "\nSearch completed. Total occurrences found: " << total_count << std::endl;
    }

}

int main(int argc, char *argv[])
{
    Search::PrintHeader();

    if (argc < 3)
    {
        Search::PrintUsage(argv[0]);
        return 1;
    }

    std::string zip_filename = argv[1];
    std::vector<std::string> keywords;

    for (int i = 2; i < argc; ++i)
    {
        if (std::string(argv[i]) == "-i")
        {
            Search::case_insensitive = true;
        }
        else
        {
            keywords.push_back(argv[i]);
        }
    }

    if (Search::case_insensitive)
    {
        for (auto &keyword : keywords)
        {
            std::ranges::transform(keyword, keyword.begin(),
                                   [](unsigned char c)
                                   { return std::tolower(c); });
        }
    }

    try
    {
        Search::SearchInZip(zip_filename, keywords);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}