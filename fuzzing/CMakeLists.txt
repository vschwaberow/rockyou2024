option(BUILD_FUZZERS "Build libFuzzer targets (requires Clang)" OFF)

if(BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_executable(fuzz_zip_search fuzz_zip_search.cc)
  target_link_libraries(fuzz_zip_search PRIVATE search_core)
  target_compile_options(fuzz_zip_search PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
  target_link_options(fuzz_zip_search PRIVATE -fsanitize=fuzzer,address,undefined)

  message(STATUS "Fuzzers enabled (run with ./fuzz_zip_search -help)")
else()
  message(STATUS "Fuzzers disabled (use Clang + -DBUILD_FUZZERS=ON)")
endif()
