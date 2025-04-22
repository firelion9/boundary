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
cd cmake-build
cmake .. -DLLVM_ROOT="$LLVM_PATH"
cd ..
cmake --build cmake-build
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/cmake-build lit --path $LLVM_PATH/bin -v test
