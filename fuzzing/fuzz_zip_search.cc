#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

import rockyou.search_engine;
import rockyou.zip_reader;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static const char* tmp = "/tmp/fuzz_rockyou.zip";
  {
    std::FILE* f = std::fopen(tmp, "wb");
    if (!f)
      return 0;
    std::fwrite(data, 1, size, f);
    std::fclose(f);
  }

  auto idx = rockyou::BuildZipIndex(tmp);
  (void)idx;

  rockyou::SearchOptions opts;
  opts.case_insensitive = (size % 2) == 0;
  opts.quiet = (size % 3) == 0;
  opts.json = (size % 5) == 0;
  opts.limit = (size % 7) ? std::optional<int>(size % 100) : std::nullopt;
  opts.context_size = size % 20;

  std::string keyword = "fuzz";
  if (size > 10) {
    keyword.assign(reinterpret_cast<const char*>(data), std::min<size_t>(size, 32));
  }

  auto res = rockyou::SearchZip(tmp, keyword, opts);
  (void)res;

  std::filesystem::remove(tmp);

  return 0;
}
