cmake_minimum_required(VERSION 3.14)

project(search VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CONFIGURATION_TYPES Debug Release)

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


add_executable(search src/Search.cc)

target_link_libraries(search PRIVATE minizip)

target_include_directories(search PRIVATE
  ${CMAKE_SOURCE_DIR}/vendor/minizip
  ${CMAKE_SOURCE_DIR}/vendor/minizip/zlib
  ${CMAKE_SOURCE_DIR}/vendor/tbb/include
)

target_compile_options(search PRIVATE
  $<$<CONFIG:Release>:-Os>
  $<$<CONFIG:Debug>:-Og>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(search PRIVATE
    $<$<CONFIG:Release>:-s>
  )
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

set_target_properties(search PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
