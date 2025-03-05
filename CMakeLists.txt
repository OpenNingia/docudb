cmake_minimum_required(VERSION 3.20)

# options
option(BUILD_TESTS "Build the test suite" ON)
if(BUILD_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
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
