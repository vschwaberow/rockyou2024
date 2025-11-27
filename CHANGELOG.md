# Changelog

## [Unreleased]

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
