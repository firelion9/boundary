#!/usr/bin/env bash

source ./build.sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/cmake-build lit --path $LLVM_PATH/bin -v test
