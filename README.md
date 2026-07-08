# rockyou2024

rockyou2024 is a command-line tool (native C++26 modules) that searches the rockyou2024 wordlist directly inside the ZIP archive — no unpacking required. Features both literal keyword search and powerful regex pattern matching for security research and password policy analysis.

## Quick start

Prebuilt archives ship with each GitHub release. Download the one that matches your platform and unzip it somewhere convenient.

### Linux / macOS

```bash
download_url=https://github.com/vschwaberow/rockyou2024/releases/download/v0.6.0/search-linux-v0.6.0.zip
curl -LO "$download_url"
unzip search-linux-v0.6.0.zip -d rockyou2024-linux
```

Replace the URL with the macOS package when needed (`https://github.com/vschwaberow/rockyou2024/releases/download/v0.6.0/search-macos-v0.6.0.zip`).

### Windows

```powershell
Invoke-WebRequest -Uri https://github.com/vschwaberow/rockyou2024/releases/download/v0.6.0/search-windows-v0.6.0.zip -OutFile search-windows-v0.6.0.zip
Expand-Archive -Path search-windows-v0.6.0.zip -DestinationPath rockyou2024-windows
```

## Usage

From the extracted folder:

```bash
./search <zip_file> <keyword> [-i] [--quiet|--json] [--limit N] [--per-file-limit N] [--threads N] [--chunk BYTES] [--context CHARS] [--checksum sha256:HEX|blake3:HEX] [--highlight] [--regex] [--regex-mode MODE]
./search --interactive
```

PowerShell is similar:

```powershell
.
search.exe <zip_file> <keyword> [-i] [--quiet|--json] [--limit N] [--per-file-limit N] [--threads N] [--chunk BYTES] [--context CHARS] [--checksum sha256:HEX|blake3:HEX] [--highlight] [--regex] [--regex-mode MODE]
.
search.exe --interactive
```

The tool exits with `0` on success. On errors you’ll get a short status message explaining what went wrong.

## Regex Pattern Search

Enable regex mode with `--regex` to search for patterns instead of literal keywords:

```bash
# Find email patterns
./search rockyou2024.zip --regex ‘[a-z]+@[a-z]+\.[a-z]+’

# Find passwords with policy violations (e.g., ends with 3+ digits)
./search rockyou2024.zip --regex ‘^[A-Za-z].*[0-9]{3}$’

# Case-insensitive regex with JSON output
./search rockyou2024.zip --regex -i --json ‘pass.*[0-9]+’

# Use different regex engines
./search rockyou2024.zip --regex --regex-mode grep ‘http://www\.’

# Complex pattern with alternation
./search rockyou2024.zip --regex ‘(admin|root|password).*[0-9]’
```

Supported regex modes: `ecmascript` (default), `awk`, `grep`, `egrep`

## Build it yourself

### Recommended (using presets)

```bash
# Development (sanitizers, analysis, tests)
cmake --preset debug
cmake --build build/debug -j
ctest --preset default --test-dir build/debug

# With coverage (90 % line / 80 % branch gate)
cmake --preset debug -DROCKYOU_ENABLE_COVERAGE=ON
cmake --build build/debug --target coverage

# Fuzzing (libFuzzer + ASan/UBSan for ZIP + search paths)
cmake --preset debug -DBUILD_FUZZERS=ON
cmake --build build/debug --target fuzz_zip_search
./build/debug/fuzz_zip_search -max_len=100000 corpus/

# Release (optimized)
cmake --preset release
cmake --build build/release -j

# One-command helper
./build_and_test.sh debug     # also supports release | asan
```

### Classic way (still works)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

The binary ends up at `build/debug/bin/search` or `build/release/bin/search`.

## Release notes

See [`CHANGELOG.md`](CHANGELOG.md) for what changed in each version.

## Acknowledgements

Original C++ port by Mike Madden (<https://github.com/mikemadden42/rockyou2024>)
