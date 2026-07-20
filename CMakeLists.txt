cmake_minimum_required(VERSION 3.16)
project(bitmap_comparison CXX)

# No external dependencies: a C++17 compiler and CMake are all you need.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Always measure in Release. A Debug build is one to two orders of magnitude
# slower and the backends do not degrade uniformly, so Debug numbers say
# nothing about the representations.
option(BENCH_NATIVE "Compile for this exact CPU (-march=native)" OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
  if(BENCH_NATIVE)
    add_compile_options(-march=native)
  endif()
endif()

add_library(bench_common STATIC
  common/bitmap_io.cpp
  common/query.cpp
)
target_include_directories(bench_common PUBLIC ${CMAKE_SOURCE_DIR})

add_library(bench_backends STATIC
  backends/raw_backend.cpp
  backends/roaring_backend.cpp
  backends/roaring_bitmap.cpp
  backends/wah_backend.cpp
)
target_include_directories(bench_backends PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(bench_backends PUBLIC bench_common)

add_executable(generate_queries tools/generate_queries.cpp)
target_link_libraries(generate_queries PRIVATE bench_common)

add_executable(bitmap_bench benchmarks/bitmap_bench.cpp)
target_link_libraries(bitmap_bench PRIVATE bench_common bench_backends)

# Task 1: your dataset generator goes here, e.g.
#
#   add_executable(generate_dataset tools/generate_dataset.cpp)
#   target_link_libraries(generate_dataset PRIVATE bench_common)
#
# bench_common gives you write_metadata() / write_bitmap() from
# common/bitmap_io.hpp, which produce exactly the format the benchmark reads.
