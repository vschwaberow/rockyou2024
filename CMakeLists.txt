cmake_minimum_required(VERSION 3.30)

project(rockyou2024 VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ROCKYOU_USE_CPP26_MODULES "Build using native C++26 modules (recommended)" ON)

if(ROCKYOU_USE_CPP26_MODULES)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
  message(STATUS "C++26 modules source active (scanner ON)")
else()
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
  message(STATUS "C++26 modules disabled")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
find_program(CLANG_FORMAT_EXE NAMES clang-format REQUIRED)
find_program(CPPCHECK_EXE NAMES cppcheck REQUIRED)
find_program(IWYU_EXE NAMES include-what-you-use)
find_program(CCACHE_EXE NAMES ccache)

if(CCACHE_EXE)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXE}")
  message(STATUS "ccache enabled")
else()
  message(STATUS "ccache not found - builds will be slower")
endif()

if(NOT IWYU_EXE)
  message(STATUS "IWYU not found - include-what-you-use checks disabled")
endif()

include(CTest)
include(FetchContent)

function(fetch_if_not_exists name git_repo git_tag)
  if(NOT EXISTS ${CMAKE_SOURCE_DIR}/vendor/${name})
    FetchContent_Declare(
      ${name}
      GIT_REPOSITORY ${git_repo}
      GIT_TAG ${git_tag}
      SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/${name}
    )
    FetchContent_MakeAvailable(${name})
  else()
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/${name})
  endif()
endfunction()

fetch_if_not_exists(minizip
  https://github.com/nmoinvaz/minizip.git
  4.0.7
)

if(EXISTS ${CMAKE_SOURCE_DIR}/vendor/blake3/c)
  message(STATUS "Using local BLAKE3 checkout from vendor/blake3")
  add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/blake3/c ${CMAKE_BINARY_DIR}/blake3-build EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(
    blake3
    GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
    GIT_TAG        1.8.5
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  c
  )
  FetchContent_MakeAvailable(blake3)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

find_package(OpenSSL REQUIRED)
add_library(search_core STATIC)

target_sources(search_core
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
      src/rockyou.messages.cppm
      src/rockyou.error_handling.cppm
      src/rockyou.search_engine.cppm
      src/rockyou.terminal_ui.cppm
      src/rockyou.zip_reader.cppm
  PRIVATE
    src/search_engine.cc
    src/terminal_ui.cc
    src/zip_reader.cc
)

target_include_directories(search_core PUBLIC
  ${CMAKE_SOURCE_DIR}/vendor/minizip
  ${CMAKE_SOURCE_DIR}/vendor/minizip/zlib
  ${CMAKE_SOURCE_DIR}/vendor/minizip/third_party/zlib
)

target_link_libraries(search_core PUBLIC minizip OpenSSL::Crypto BLAKE3::blake3)

if(IWYU_EXE)
  set_target_properties(search_core PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXE}")
endif()

target_compile_options(search_core PUBLIC
  $<$<CONFIG:Release>:-Os -DNDEBUG>
  $<$<CONFIG:Debug>:-Og -g>
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror -Wno-unused-function -Wno-deprecated-declarations -Wno-sign-conversion -Wno-redundant-move>
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(USE_SANITIZERS)
  target_compile_options(search_core PUBLIC -fsanitize=${USE_SANITIZERS} -fno-omit-frame-pointer)
  target_link_options(search_core PUBLIC -fsanitize=${USE_SANITIZERS})
endif()

add_executable(search src/main.cc)
target_link_libraries(search PRIVATE search_core)
target_compile_options(search PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror -Wno-unused-function -Wno-deprecated-declarations -Wno-sign-conversion -Wno-redundant-move>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(search PRIVATE $<$<CONFIG:Release>:-s>)
endif()

set_target_properties(search PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(search_tests tests/search_engine_test.cc)
target_link_libraries(search_tests PRIVATE search_core GTest::gtest_main)
target_compile_options(search_tests PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror -Wno-unused-function -Wno-deprecated-declarations -Wno-sign-conversion -Wno-redundant-move>
)
target_compile_definitions(search_tests PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
add_test(NAME search_tests COMMAND search_tests)

#C++ 26 / Jason Turner style : coverage instrumentation for Debug builds
#(enables gcovr reports and 90 % hard gate)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  option(ROCKYOU_ENABLE_COVERAGE "Build with coverage instrumentation (Debug only)" OFF)
  if(ROCKYOU_ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(COVERAGE_FLAGS -fprofile-arcs -ftest-coverage)
    target_compile_options(search_core PRIVATE ${COVERAGE_FLAGS})
    target_link_options(search_core PRIVATE ${COVERAGE_FLAGS})
    target_compile_options(search PRIVATE ${COVERAGE_FLAGS})
    target_link_options(search PRIVATE ${COVERAGE_FLAGS})
    target_compile_options(search_tests PRIVATE ${COVERAGE_FLAGS})
    target_link_options(search_tests PRIVATE ${COVERAGE_FLAGS})
    message(STATUS "Coverage instrumentation enabled for Debug build")
  endif()
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/benchmark/CMakeLists.txt)
  add_subdirectory(benchmark)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/fuzzing/CMakeLists.txt)
  add_subdirectory(fuzzing)
endif()

add_custom_target(format
  COMMAND ${CLANG_FORMAT_EXE} -i -style=file
          ${CMAKE_SOURCE_DIR}/src/*.cc
          ${CMAKE_SOURCE_DIR}/src/*.cppm
          ${CMAKE_SOURCE_DIR}/tests/*.cc
          ${CMAKE_SOURCE_DIR}/CMakeLists.txt
  COMMENT "Formatting all source with clang-format (LLVM style)"
)

find_program(GCOVR_EXE NAMES gcovr)
if(GCOVR_EXE)
  add_custom_target(coverage
    COMMAND ${GCOVR_EXE} -r ${CMAKE_SOURCE_DIR} --html --html-details -o coverage.html
            --exclude 'vendor/.*' --exclude 'build/.*' --exclude 'tests/.*'
            --fail-under-line 90 --fail-under-branch 80
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report (90% line / 80% branch hard gate, requires Debug + coverage build)"
    VERBATIM
  )
endif()

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_VENDOR "Volker Schwaberow")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Rockyou2024 search tool (C++26 modules edition)")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

install(TARGETS search
  RUNTIME DESTINATION bin
  COMPONENT applications
)
include(CPack)

message(STATUS "rockyou2024 C++26 configured with modules, clang-tidy, cppcheck, -Werror")
message(STATUS "Use: cmake --preset debug && cmake --build build/debug && ctest --preset default")
