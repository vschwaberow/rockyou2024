# Changelog

## [0.7.0] - 2026-07-09

### New output modes

- `--count` prints only the total match count — a bare number on
  stdout, or `{"total": N}` when combined with `--json`. Useful for
  scripting and quick existence checks.
- `--stats` prints search statistics to stderr (stdout is unaffected):
  entries searched/skipped/errored, bytes decompressed, per-phase
  timing (init, search, finalize, total), throughput in MB/s, and a
  top-5 entries-by-hits ranking. Combinable with `--json`, `--quiet`,
  `--count`, and `--regex`.

### Tests

Two new tests for `--count` (plain number, JSON variant) and two for
`--stats` (stderr content, stdout isolation).

## [0.6.0] - 2026-06-30

### Regex pattern search

- `--regex` flag enables regex pattern matching instead of literal
  keyword search.
- `--regex-mode` selects the regex syntax: `ecmascript` (default),
  `awk`, `grep`, `egrep`.
- New module `rockyou.regex_engine.cppm` handles compilation, matching,
  and a hybrid fast path (Boyer-Moore prefix rejection + regex
  verification).
- `SearchFileRegex` integrates regex into the parallel search pipeline
  with chunk-overlap streaming.
- JSON output includes `regex` and `regex_mode` fields.
- 17 new regex tests covering character classes, quantifiers,
  anchors, alternation, and all four syntax modes.

## [0.5.0] - 2026-06-28

### Concurrency overhaul

The internal worker pool that parallelises ZIP entry search has been
rewritten. CLI, JSON schema, and exit codes are unchanged.

**What's better**

- `--limit` is now actually respected mid-search. Previously every
  entry was scanned to completion even after the global limit was
  already satisfied; on a 100 GB wordlist with `--limit 1` the tool
  now stops almost immediately after the first match.
- A `std::mutex` on the per-entry error list is gone. Each worker
  writes lock-free into a pre-sized vector at its own index.
- The keyword is stored once and shared via `std::string_view`
  instead of being copied per worker thread.
- The dead `total_count` atomic (incremented but never read) has
  been removed.
- The `hardware_concurrency() == 0` fallback is now a named
  constant `kDefaultThreadCountFallback` in `messages.cppm`.
- Workers are `std::jthread`s with cooperative shutdown via
  `std::stop_source`; they no longer risk `std::terminate()` on
  exceptions or require an explicit `join()` loop.

**Technical changes**

- `std::jthread` + `std::stop_source` / `std::stop_token` replace
  `std::thread` + manual `join()`. A `std::latch` waits for all
  workers.
- Work claiming switched from per-entry `fetch_add(1)` to batched
  `fetch_add(kWorkBatchSize)` with `kWorkBatchSize = 8`.
- A new `std::atomic<int> remaining_limit` carries the live `--limit`
  budget. Workers check it before each entry; when exhausted they
  fill their remaining slots with empty `truncated = true` results
  and request stop.
- `SearchFile` and `SearchPositions` now take `std::string_view`,
  removing the last forced keyword copy inside the search loop.

**Tests**

Four new tests cover the new threading behaviour:
`SearchZipRespectsExplicitThreadCount`,
`SearchZipRespectsHardwareFallbackThreadCount`,
`SearchZipDeterministicWithLimitAcrossThreads` (pin down JSON output
under `--limit 1` with `thread_count = 4`), and
`SearchZipProducesFullCountWithoutLimit`.

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
