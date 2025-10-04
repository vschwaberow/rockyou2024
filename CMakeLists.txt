cmake_minimum_required(VERSION 3.16)

project(search VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES Debug Release)

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

enable_testing()

fetch_if_not_exists(minizip
  https://github.com/nmoinvaz/minizip.git
  4.0.7
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_library(search_core STATIC
  src/search_engine.cc
  src/terminal_ui.cc
  src/zip_reader.cc
)

target_include_directories(search_core PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/vendor/minizip
  ${CMAKE_SOURCE_DIR}/vendor/minizip/zlib
  ${CMAKE_SOURCE_DIR}/vendor/tbb/include
)

target_precompile_headers(search_core PUBLIC include/rockyou/pch.h)

target_link_libraries(search_core PUBLIC minizip)

target_compile_options(search_core PUBLIC
  $<$<CONFIG:Release>:-Os>
  $<$<CONFIG:Debug>:-Og>
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(search src/main.cc)

target_link_libraries(search PRIVATE search_core)

target_precompile_headers(search PRIVATE include/rockyou/pch.h)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(search PRIVATE
    $<$<CONFIG:Release>:-s>
  )
endif()

set_target_properties(search PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(search_tests
  tests/search_engine_test.cc
)

target_link_libraries(search_tests PRIVATE search_core GTest::gtest_main)

target_precompile_headers(search_tests PRIVATE include/rockyou/pch.h)

target_compile_definitions(search_tests PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")

add_test(NAME search_tests COMMAND search_tests)

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_VENDOR "Volker Schwaberow")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Rockyou2024 search tool")
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
