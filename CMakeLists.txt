cmake_minimum_required(VERSION 3.20)

# options
option(BUILD_TESTS "Build the test suite" OFF)
option(BUILD_BENCH "Build the benchmark suite" OFF)
option(BUILD_EXAMPLES "Build the examples" OFF)

if(BUILD_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif(BUILD_TESTS)

if(BUILD_BENCH)
  list(APPEND VCPKG_MANIFEST_FEATURES "bench")
endif(BUILD_BENCH)

if(BUILD_EXAMPLES)
  list(APPEND VCPKG_MANIFEST_FEATURES "examples")
endif(BUILD_EXAMPLES)

string(TIMESTAMP _NOW "%Y-%m-%dT%H:%M:%SZ")
mark_as_advanced(_NOW)

set(DOCUDB_VERSION "1.0.0")
set(DOCUDB_VERSION_DATE ${_NOW})

# Set the project name
project(docudb VERSION ${DOCUDB_VERSION} LANGUAGES CXX)

# c++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

IF(MSVC)
    SET(CMAKE_CXX_FLAGS "/EHsc")
ENDIF(MSVC)

# try to find system sqlite3
find_package(SQLite3)

if (NOT SQLite3_FOUND)
    # get sqlite dependency from vcpkg
    find_package(unofficial-sqlite3 CONFIG)
    if (NOT unofficial-sqlite3_FOUND)
        message(FATAL_ERROR "SQLite3 not found. Please install SQLite3 or use vcpkg.")
    endif()

    message(STATUS "Using SQLite3 from Vcpkg")
else()
    message(STATUS "Using System SQLite3")
endif()

# Test suite dependencies
if (BUILD_TESTS)
    find_package(doctest CONFIG REQUIRED)
endif(BUILD_TESTS)

# Benchmarks dependencies
if (BUILD_BENCH)
    find_package(benchmark CONFIG REQUIRED)
endif(BUILD_BENCH)

if (BUILD_EXAMPLES)
    find_package(FLTK CONFIG REQUIRED)
endif(BUILD_EXAMPLES)

# configure version file
configure_file(src/docudb_version.h.in src/docudb_version.h @ONLY)

# Add the library
add_library(docudb src/docudb.cpp src/sqlite_extensions.cpp)
target_include_directories(docudb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(docudb PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)

if (SQLite3_FOUND)
    target_link_libraries(docudb PUBLIC SQLite::SQLite3)
else()
    target_link_libraries(docudb PUBLIC unofficial::sqlite3::sqlite3)
endif()

if (BUILD_TESTS)
    include(CTest)
    include(doctest)
    enable_testing()
    add_executable(docudb_test doctest/tests.cpp)
    target_link_libraries(docudb_test PRIVATE docudb doctest::doctest)
    doctest_discover_tests(docudb_test)

    # Enable code coverage if supported
    option(ENABLE_COVERAGE "Enable code coverage reporting" ON)
    if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        target_compile_options(docudb PUBLIC --coverage)
        target_link_options(docudb PUBLIC --coverage)

        # Add custom target for lcov coverage report
        find_program(LCOV_EXEC lcov)
        find_program(GENHTML_EXEC genhtml)
        if(LCOV_EXEC AND GENHTML_EXEC)
            add_custom_target(coverage
                COMMAND ${LCOV_EXEC} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV_EXEC} --remove coverage.info '/usr/*' '*/doctest/*' --output-file coverage.info.cleaned
                COMMAND ${GENHTML_EXEC} -o coverage_html coverage.info.cleaned
                COMMENT "Generating lcov coverage report in coverage_html/"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                DEPENDS docudb_test
            )
        else()
            message(WARNING "lcov/genhtml not found: coverage target will not be available")
        endif()        
    endif()    
endif()

if (BUILD_BENCH)
    add_executable(docudb_bench bench/bench.cpp)
    target_link_libraries(docudb_bench PRIVATE docudb benchmark::benchmark benchmark::benchmark_main)
endif()

if (BUILD_EXAMPLES)
    message("Building examples")
    add_subdirectory(examples/docui)
endif(BUILD_EXAMPLES)

add_subdirectory(doc)
