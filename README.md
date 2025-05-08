Boundary
========

LLVM pass library for detecting and marking code execution mode.

Building
========

Install LLVM and LLVM LLD:
```bash
sudo apt install llvm-18 lld-18
```

Set environmental variable `LLVM_DIR` to LLVM installation dir (default value is `/usr/lib/llvm-18`) and run `./build.sh`.
Alternatively, you can manually run cmake. Minimum supported version is LLVM 15. 
```bash
export LLVM_DIR=/usr/lib/llvm-18 
cmake ./ -B cmake-build -DLLVM_ROOT="$LLVM_PATH"
cmake --build cmake-build
```

Using Boundary
==============

Boundary is LLVM pass plugin library, so you can use it with any LLVM tool.

For clang, you should prefix any boundary option with `-mllvm`.

```bash
clang -fplugin=./cmake-build/libBoundary.so -fpass-plugin=./cmake-build/libBoundary.so -mllvm --boundary-pass="print" --load-function-profiles="stdlib++-io-profile.txt" ...
```

```bash
opt -load-pass-plugin ./cmake-build/libBoundary.so -passes="print<boundary>" --load-function-profiles="stdlib++-io-profile.txt" ...
```

This repository also provides an auxiliary script `clang-with-boundary.sh` which automatically compiles the plugin and runs clang with correct minimum plugin setup.
Using it, we can rewrite the previous example with clang in a shorter way (note that it also uses lld linker):
```bash
./clang-with-boundary.sh -mllvm --boundary-pass="print" --load-function-profiles="stdlib++-io-profile.txt" ...
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
To do so, you should compile your program with `-fprofile-generate` flag (it is not necessary to use boundary plugin on this step),
run compiled program, merge raw profiling data with `llvm-profdata merge *.profraw > prof.profdata` 
and then pass resulting `prof.profdata` to new compilation with boundary using `-fprofile-use=prof.profdata` option.
```bash
clang -fprofile-generate app.cpp -o app
# clean old profiling data
rm *.profraw
# You may wish to run the app more then once
./app
llvm-profdata merge *.profraw > prof.profdata
./clang-with-boundary.sh -fprofile-use=prof.profdata -mllvm --boundary-pass="instrument" ...
```

Supported CLI options
==============
+ `--load-function-profiles=<file>` -- loads complexity information for external function from specified file. 
  May be used multiple times
+ `--boundary-analysis-direction=<complexity|bfs|dfs>` -- sets call graph analysis algorithm. 
  Preferred (and default) value is `complexity`, because 2 other legacy options are unable to gather complete complexity information
+ `--boundary-pass=<print|dump|instrument>` -- enables output passes. May be used multiple times to enable multiple passes. 
  For tools like opt prefer using native option `-passes` with syntax like `-passes=print<boundary>` (angle brackets are part of syntax here). Available values are:
  + `print` -- outputs a human-readable function complexity table to stderr
  + `dump` -- writes function complexity profile under directory specified by `--mode-dump-dir`. The profile can be then passed to `--load-function-profiles`
  + `instrument` -- inserts intrinsics to result code based on computed function complexity values
+ `--mode-dump-dir` -- specifies output directory for `dump` pass
+ `--complexity-threshold` -- specified the minimum total function complexity to be process by `instrument` pass
+ `--intrinsic-<io|memory|cpu>-<enter|exit>=<function_name>` -- sets corresponding intrinsic to specified value for `instrument` pass. 
  For example, `--intrinsic-io-enter=io_enter` tells boundary to insert `io_enter()` call in the beginning of every function classified as IO.
  Target function should follow C call convention, have signature `void <function_name>()` and be thread-safe (unless the program to instrument is not single-thread itself)

Testing
=======

This repository provides a bash script to build the plugin and execute tests on it. You may require to install `lit` before using it:
```bash
pip install lit
```
Now, you can set `LLVM_DIR` and run tests:
```bash
LLVM_DIR=</llvm/dir> ./run-test.sh
```

Alternatively, you can manually compile boundary and then run tests using `lit` directly: 

```bash
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:</absolute/path/to/libBoundary.so/directory> lit --path </llvm/dir>/bin/ -v test/
```

Be sure to use the same version of LLVM when compiling boundary and when using it as plugin or running tests.
Otherwise, you are likely to catch a segmentation fault during LLVM initialization.

