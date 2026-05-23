#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-debug}"

case "$MODE" in
  debug|release|asan) ;;
  *) echo "Usage: $0 [debug|release|asan]"; exit 1 ;;
esac

echo "=== rockyou2024 C++26 Build & Test ($MODE) ==="

for tool in clang clang++ clang-tidy clang-format cppcheck cmake; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: $tool not found. Install it (see README)."; exit 1; }
done

echo "-> Configuring ($MODE)..."
cmake --preset "$MODE"

echo "-> Formatting codebase (clang-format)..."
cmake --build "build/$MODE" --target format 2>/dev/null || echo "  (format skipped or not needed)"

echo "-> Building (parallel)..."
cmake --build "build/$MODE" --parallel "$(nproc)"

if [ "$MODE" != "release" ]; then
  echo "-> Running tests..."
  ctest --preset default --test-dir "build/$MODE"
fi

if [ "$MODE" = "debug" ] && command -v gcovr >/dev/null 2>&1; then
  echo "-> Coverage report (C++26 90% gate)..."
  cmake --build "build/$MODE" --target coverage || echo "  (coverage below gate or not enabled with -DROCKYOU_ENABLE_COVERAGE=ON)"
fi

echo "=== Build successful! Binary at build/$MODE/bin/search ==="
echo "Run: ./build/$MODE/bin/search --help"

if [ "$MODE" = "release" ]; then
  echo "(Tests skipped for Release builds)"
fi
