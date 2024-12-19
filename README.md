Boundary
========

LLVM pass library for detecting and marking code execution mode.

Building
========

Install LLVM and LLVM LLD:
```bash
sudo apt install llvm-18 lld-18
```

Set environmental variable `LLVM_DIR` to LLVM installation dir, run cmake and then make. Minimum supported version is LLVM 15. 
```bash
export LLVM_DIR=/usr/lib/llvm-18 
cmake ./ -B cmake-build
cd cmake-build
make
```

Using Boundary
==============

Boundary is LLVM pass plugin library, so you can use it with any LLVM tool.

For clang, you should prefix any boundary option with `-mllvm`.

```bash
clang -fplugin=./libBoundary.so -fpass-plugin=./libBoundary.so -mllvm --boundary-pass="print" --load-function-profiles="../stdlib++-io-profile.txt" ...
```

```bash
opt -load-pass-plugin ./libBoundary.so -passes="print<boundary>" --load-function-profiles="../stdlib++-io-profile.txt" ...
```

The primary pass of the library is instrument pass (`--boundary-pass="instrument"` or `-passes="instrument<boundary>"`).
It will insert calls to functions specified by `--intrinsic-<type>-<enter or exit>` into beginnings and ends of matching code blocks.

For more accurate results, you may wish to use function profiles for external code (`--load-function-profiles`).
An example profile for C++ stdlib is provided in file stdlib++-io-profile.txt.
You can generate such profiles using full link-time optimization (flto) `dump` pass.
It is enabled by default when flto is active, but activating flto is a bit tricky:

```bash
clang -fplugin=path/to/libBoundary.so -fpass-plugin=path/to/libBoundary.so --ld-path=$LLVM_DIR/bin/ld.lld -flto=full -Xlinker --load-pass-plugin=path/to/libBoundary.so ...
```

Profile will be written under `function_modes` folder.

For even more accurate results you may wish to utilize profiling data.
To do so, you should compile your program with `--fprofile-generate` flag (it is not necessary to use boundary plugin on this step),
run compiled program, merge raw profiling data with `llvm-profdata merge *.profraw > prof.profdata` 
and then pass resulting `prof.profdata` to new compilation with boundary using `--fprofile-use=prof.profdata` option.

Testing
=======

You can run test from root directory of this repository in such way (requires `lit` python package or portable version from `$LLVM_DIR/build/utils/lit`): 

```bash
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/cmake-build lit --path $LLVM_DIR/bin/ -v test/
```