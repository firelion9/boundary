; Test Memory blocks instrumentation in simple case.
; RUN: opt < %s -load-pass-plugin libBoundary.so -passes="instrument<boundary>" --load-function-profiles=stdlib++-io-profile.txt --intrinsic-memory-enter="memory_enter" --intrinsic-memory-exit="memory_exit" -S | FileCheck %s --check-prefix=USE

define dso_local void @cpy_off(ptr %in, ptr %out, i32 %out_off) {
; USE: call void @memory_enter
  %out0 = getelementptr i32, i32* %out, i32 %out_off
  %val = load i32, i32* %in
  store i32 %val, i32* %out0
  ret void
; USE: call void @memory_exit
}

define dso_local void @memory_enter() {
  ret void
}

define dso_local void @memory_exit() {
  ret void
}
