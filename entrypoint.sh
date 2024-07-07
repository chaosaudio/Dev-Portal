#!/bin/bash

cd /workdir
mkdir -p build && cd build \
&& cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DBUILD_BENCH=ON ../ \
&& cmake --build . \
&& make install DESTDIR="/workdir"
