cmake_minimum_required(VERSION 3.10)

# set the project name
project(tests)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add the executable
add_executable(benchmark-plugin benchmark_plugin.cpp)

# Set the flags for this directory
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-O2")
message("CMAKE_CXX_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_SHARED_LINKER_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

# include
include_directories(benchmark-plugin
    ../resources
    ../common)

# link against required libraries
target_link_libraries(benchmark-plugin PRIVATE dl)

# config install
install(TARGETS benchmark-plugin
    DESTINATION bins/tests/benchmark)
