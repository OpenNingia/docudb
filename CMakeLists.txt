cmake_minimum_required(VERSION 3.20)

# Set the project name
project(docudb)

find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Add the library
add_library(docudb src/docudb.cpp)
target_include_directories(docudb PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(docudb PUBLIC unofficial::sqlite3::sqlite3)

# Test executable
add_executable(test test/test.cpp)
target_link_libraries(test PRIVATE docudb)