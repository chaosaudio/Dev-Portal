cmake_minimum_required(VERSION 3.10)

# set the project name
project(aida-x-tests)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the flags for this directory
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-O2")
message("CMAKE_CXX_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_SHARED_LINKER_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

# Function to add an executable, set include directories, link libraries, and install
function(add_executable_and_setup target_name source_file)
    add_executable(${target_name} ${source_file})
    include_directories(${target_name} ../../resources ../../common)
    target_link_libraries(${target_name} PRIVATE dl)
    install(TARGETS ${target_name} DESTINATION bins/tests/benchmark)
endfunction()

# Add the executables
add_executable_and_setup(benchmark-player benchmark_player.cpp)
add_executable_and_setup(benchmark-convolver benchmark_convolver.cpp)
