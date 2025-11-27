# rockyou2024

rockyou2024 is a fast, C++23 command-line helper that hunts through the rockyou2024 wordlist — even while it is still zipped — so you can check candidate passwords without unpacking gigantic archives.

## Why it’s handy
- Works directly on the official ZIP archive using minizip.
- Respects Google’s C++ style (no exceptions, explicit status codes).
- Offers an interactive prompt or a plain CLI with optional case-insensitive search.

## Quick start
Prebuilt archives ship with each GitHub release. Download the one that matches your platform and unzip it somewhere convenient.

### Linux / macOS
```bash
download_url=https://github.com/vschwaberow/rockyou2024/releases/download/v0.2.0/search-linux-v0.2.0.zip
curl -LO "$download_url"
unzip search-linux-v0.2.0.zip -d rockyou2024-linux
```
Replace the URL with the macOS package when needed (`https://github.com/vschwaberow/rockyou2024/releases/download/v0.2.0/search-macos-v0.2.0.zip`).

### Windows
```powershell
Invoke-WebRequest -Uri https://github.com/vschwaberow/rockyou2024/releases/download/v0.2.0/search-windows-v0.2.0.zip -OutFile search-windows-v0.2.0.zip
Expand-Archive -Path search-windows-v0.2.0.zip -DestinationPath rockyou2024-windows
```

## Usage
From the extracted folder:
```bash
./search <zip_file> <keyword> [-i] [--quiet|--json] [--limit N] [--per-file-limit N] [--threads N] [--chunk BYTES] [--context CHARS] [--checksum sha256:HEX|blake3:HEX] [--highlight]
./search --interactive
```
PowerShell is similar:
```powershell
.
search.exe <zip_file> <keyword> [-i] [--quiet|--json] [--limit N] [--per-file-limit N] [--threads N] [--chunk BYTES] [--context CHARS] [--checksum sha256:HEX|blake3:HEX] [--highlight]
.
search.exe --interactive
```
The tool exits with `0` on success. On errors you’ll get a short status message explaining what went wrong.

## Build it yourself
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Release notes
See [`CHANGELOG.md`](CHANGELOG.md) for what changed in each version.

---
Version 0.2.0 · MIT licensed · Built by Volker Schwaberow <volker@schwaberow.de>

## Acknowledgements

Original C++ port by Mike Madden (https://github.com/mikemadden42/rockyou2024)
