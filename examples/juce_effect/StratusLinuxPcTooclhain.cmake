# Toolchain to build binaries for Stratus on Ubuntu computer
# Usage: cmake .. -DCMAKE_TOOLCHAIN_FILE=StratusLinuxPcTooclhain.cmake

set(CMAKE_SYSTEM_NAME  Linux)
set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

add_compile_options(-fPIC -shared -O2 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math)
