#!/usr/bin/env bash

source ./build.sh
"$LLVM_PATH/bin/clang" -fplugin=./cmake-build/libBoundary.so -fpass-plugin=./cmake-build/libBoundary.so \
                       -fuse-ld="$LLVM_PATH/bin/ld.lld" -Xlinker --load-pass-plugin=./cmake-build/libBoundary.so \
                       $@

