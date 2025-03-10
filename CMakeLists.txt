cmake_minimum_required(VERSION 3.20)

# options
option(BUILD_TESTS "Build the test suite" ON)
option(BUILD_BENCH "Build the benchmark suite" ON)

if(BUILD_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

if(BUILD_BENCH)
  list(APPEND VCPKG_MANIFEST_FEATURES "bench")
endif()


# Set the project name
project(docudb)

# c++ standard
set(CMAKE_CXX_STANDARD 20)

# sqlite dependency
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Test suite dependencies
if (BUILD_TESTS)
    find_package(doctest CONFIG REQUIRED)
endif(BUILD_TESTS)

# Benchmarks dependencies
if (BUILD_BENCH)
    find_package(benchmark CONFIG REQUIRED)
endif(BUILD_BENCH)

# Add the library
add_library(docudb src/docudb.cpp)
target_include_directories(docudb PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(docudb PUBLIC unofficial::sqlite3::sqlite3)

if (BUILD_TESTS)
    include(CTest)
    include(doctest)
    enable_testing()
    add_executable(docudb_test doctest/tests.cpp)
    target_link_libraries(docudb_test PRIVATE docudb doctest::doctest)
    doctest_discover_tests(docudb_test)
endif()

if (BUILD_BENCH)
    add_executable(docudb_bench bench/bench.cpp)
    target_link_libraries(docudb_bench PRIVATE docudb benchmark::benchmark benchmark::benchmark_main)
endif()
