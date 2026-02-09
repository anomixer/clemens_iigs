#!/bin/bash
mkdir -p build-em
cd build-em
emcmake cmake .. -DEMSCRIPTEN=1 -DCMAKE_BUILD_TYPE=Release
cmake --build .
echo "Build complete. To run, use: emrun host/clemens_iigs.html"
