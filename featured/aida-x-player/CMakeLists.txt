#cmake_minimum_required(VERSION 3.15)
cmake_minimum_required(VERSION 3.7.2)
project(aida-x-player VERSION 1.1.0)

set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

# if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|amd64|AMD64|i.86|x64|X64|x86|x86_64|X86)$")
#   set(RTNEURAL_XSIMD TRUE CACHE BOOL "Use RTNeural with this backend")
#   set(RTNEURAL_EIGEN FALSE CACHE BOOL "Use RTNeural with this backend")
# else()
set(RTNEURAL_XSIMD FALSE CACHE BOOL "Use RTNeural with this backend")
set(RTNEURAL_EIGEN TRUE CACHE BOOL "Use RTNeural with this backend")
# endif()

message("RTNEURAL_XSIMD in ${CMAKE_PROJECT_NAME} = ${RTNEURAL_XSIMD}, using processor type ${CMAKE_SYSTEM_PROCESSOR} and system name ${CMAKE_SYSTEM_NAME}")
message("RTNEURAL_EIGEN in ${CMAKE_PROJECT_NAME} = ${RTNEURAL_EIGEN}, using processor type ${CMAKE_SYSTEM_PROCESSOR} and system name ${CMAKE_SYSTEM_NAME}")

# set optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -static-libstdc++ -Wl,-Ofast -Wl,--as-needed -Wl,--strip-debug")
message("CMAKE_CXX_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_SHARED_LINKER_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

# add external libraries
add_subdirectory(../../modules/RTNeural ${CMAKE_CURRENT_BINARY_DIR}/RTNeural)

# configure executable
add_library(aida-x-player SHARED
    aida-x-player.cpp)

# include and link directories
include_directories(aida-x-player
    ./
    ../../resources
    ../../common
    ../../modules/RTNeural/modules/json
    ../../modules/RTNeural)

link_directories(aida-x-player
    ../../modules/RTNeural
    ../../modules/RTNeural/modules/json)

target_link_libraries(aida-x-player PRIVATE RTNeural)
set_target_properties(aida-x-player PROPERTIES PREFIX "")

# config install
install(TARGETS aida-x-player
    DESTINATION bins/aida-x-player)
