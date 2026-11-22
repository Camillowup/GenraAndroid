#!/bin/sh
# Builds libgenrabox-dev.so for the desktop test server (Linux / macOS with a C and C++ compiler).
set -e
cd "$(dirname "$0")"
C=../../app/src/main/cpp
mkdir -p build
[ -f build/drumcore.o ] || cc -std=gnu11 -O2 -w -fPIC -ffp-contract=off -DFELUCCA_VERSION='"DEV"' -I$C/drumx/jiant/build/gen -I$C/drumx/jiant/firmware/src -fvisibility=hidden -c $C/drumx/jiantcore/drumcore.c -o build/drumcore.o
c++ -std=gnu++17 -O2 -fPIC -shared -pthread capi.cpp $C/box/BoxEngine.cpp $C/genra-src/genra/Engine.cpp $C/genra-src/genra/Genres.cpp \
  $C/genra-src/genra/Arranger.cpp $C/genra-src/genra/Edit.cpp $C/genra-src/genra/Midi.cpp $C/genra-src/genra/Store.cpp \
  build/drumcore.o -o build/libgenrabox-dev.so
echo "built build/libgenrabox-dev.so"
