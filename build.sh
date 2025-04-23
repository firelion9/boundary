#!/usr/bin/env bash

if [ -z "$LLVM_PATH" ]; then
  LLVM_PATH=/usr/lib/llvm-18
fi
echo "using LLVM_PATH=\"$LLVM_PATH\""
if [ ! -d "$LLVM_PATH" ]; then
  echo "Illegal LLVM_PATH=\"$LLVM_PATH\". Please specify a valid LLVM_PATH"
  exit -1
fi
if [ ! -d cmake-build ]; then
  mkdir cmake-build
fi
cmake ./ -B cmake-build -DLLVM_ROOT="$LLVM_PATH"
cmake --build cmake-build
