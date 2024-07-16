cmake_minimum_required(VERSION 3.14)

project(search VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_compile_options(-fno-rtti)

add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

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

find_package(TBB REQUIRED)

add_executable(search src/Search.cc)

target_link_libraries(search PRIVATE
  minizip
  TBB::tbb
)

target_include_directories(search PRIVATE
  ${CMAKE_SOURCE_DIR}/vendor/minizip
)