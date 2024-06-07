#!/bin/bash

mkdir -p build_stratus && cd build_stratus
cmake .. -DCMAKE_TOOLCHAIN_FILE=StratusDockerTooclhain.cmake
cmake --build .
