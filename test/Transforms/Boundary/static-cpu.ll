; Test CPU blocks instrumentation in simple case.
; RUN: opt < %s -load-pass-plugin libBoundary.so -passes="instrument<boundary>" --load-function-profiles=stdlib++-io-profile.txt --intrinsic-cpu-enter="cpu_enter" --intrinsic-cpu-exit="cpu_exit" --complexity-threshold=0 -S | FileCheck %s --check-prefix=USE

define dso_local i32 @compute(i32 %a, i32 %b, i32 %c) {
; USE: call void @cpu_enter
  %t0 = add i32 %a, %b
  %t1 = mul i32 %t0, %c
  %t2 = add i32 %t1, %a
  %t3 = mul i32 %t2, %b
  ret i32 %t3
; USE: call void @cpu_exit
}

define dso_local void @cpu_enter() {
  ret void
}

define dso_local void @cpu_exit() {
  ret void
}
