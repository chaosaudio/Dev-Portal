#cmake_minimum_required(VERSION 3.15)
cmake_minimum_required(VERSION 3.7.2)
project(aida-x-convolver VERSION 0.9.0)

set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#set(CMAKE_C_VISIBILITY_PRESET hidden)
#set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

# set optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -static-libstdc++ -Wl,-Ofast -Wl,--as-needed -Wl,--strip-debug")
message("CMAKE_CXX_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_SHARED_LINKER_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

# configure executable
add_library(aida-x-convolver SHARED
    aida-x-convolver.cpp
    ../../common/Biquad.cpp
    ../../modules/FFTConvolver/AudioFFT.cpp
    ../../modules/FFTConvolver/FFTConvolver.cpp
    ../../modules/FFTConvolver/TwoStageFFTConvolver.cpp
    ../../modules/FFTConvolver/Utilities.cpp
    ../../modules/r8brain/pffft.cpp
    ../../modules/r8brain/r8bbase.cpp)

# include and link directories
include_directories(aida-x-convolver
    ./
    ../../resources
    ../../common
    ../../modules/dr_libs
    ../../modules/FFTConvolver
    ../../modules/r8brain)

target_link_libraries(aida-x-convolver PRIVATE pthread)
set_target_properties(aida-x-convolver PROPERTIES PREFIX "")

# config install
install(TARGETS aida-x-convolver
    DESTINATION bins/aida-x-convolver)
