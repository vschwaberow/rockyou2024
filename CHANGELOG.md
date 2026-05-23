# Changelog

## [Unreleased]

## [0.4.0] - 2026-05-23

### C++26 Migration

- Full migration to native C++26 modules (transitional global-fragment pattern with `module;` + traditional includes).
- Complete switch from legacy `Status`/`StatusOr` to `rockyou::Result<T>` (`std::expected<T, AppError>`).

### Testing & Quality (Major Expansion)

- Expanded test suite from 4 to 29 focused tests covering:
  - All `SearchZip` input validation and error paths (`Result<T>` / `MakeError`)
  - Checksum verification (SHA-256 / BLAKE3, mismatch cases)
  - Output modes: JSON structure, quiet mode, highlight, context sizing, truncation
  - Direct `ZipEntryStream` API testing (Create, Read, CloseWithStatus, error cases)
  - CLI smoke tests for the `search` binary (help, basic search, JSON, error handling, long inputs)
  - Edge and boundary cases
- All new tests follow the existing GoogleTest style and use `rockyou::Result<T>` assertions.
- Added full code coverage instrumentation for Debug builds.
- Coverage target now enforces **90 % line / 80 % branch** hard gate via gcovr.
- Introduced libFuzzer support:
  - New `fuzzing/` directory with `fuzz_zip_search` target (exercises ZIP parsing + search engine).
  - Build with `-DBUILD_FUZZERS=ON` (Clang + sanitizers).
- Updated `build_and_test.sh` and CMake to support modern quality workflows (`-DROCKYOU_ENABLE_COVERAGE=ON`, fuzzers).
- All changes implemented surgically following Karpathy guidelines and C++26 best practices (minimal diffs, `Result<T>` only, no speculative abstractions).

## [0.3.0] - 2025-11-27

- Add quiet/JSON output modes with sorting/limits and highlightable context
- Expose performance flags (threads/chunk/context) and integrity verification (SHA-256 or BLAKE3)
- Tolerate corrupt ZIP entries while reporting partial failures; interactive mode allows repeated searches
- Added CLI/E2E tests for new flags, JSON/quiet output, performance controls, checksum, and corrupt entries

## [0.2.0] - 2025-05-09

- Switched to status-based error handling aligned with Google C++ Style (no exceptions).
- Migrated CLI, search engine, and ZIP reader to the C++23 print API.
- Split implementation into modular translation units with RAII wrappers and added SPDX headers.
- Added cross-platform packaging and OS-specific installation documentation.

## [0.1.1] - 2024-04-01

- Initial public release of the search utility.
