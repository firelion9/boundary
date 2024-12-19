; Test I/O blocks instrumentation in simple case.
; RUN: opt < %s -load-pass-plugin libBoundary.so -passes="instrument<boundary>" --load-function-profiles=stdlib++-io-profile.txt --intrinsic-io-enter="io_enter" --intrinsic-io-exit="io_exit" -S | FileCheck %s --check-prefix=USE

%"class.std::ios_base::Init" = type { i8 }
%"class.std::basic_ostream" = type { ptr, %"class.std::basic_ios" }
%"class.std::basic_ios" = type { %"class.std::ios_base", ptr, i8, i8, ptr, ptr, ptr, ptr }
%"class.std::ios_base" = type { ptr, i64, i64, i32, i32, i32, ptr, %"struct.std::ios_base::_Words", [8 x %"struct.std::ios_base::_Words"], i32, ptr, %"class.std::locale" }
%"struct.std::ios_base::_Words" = type { ptr, i64 }
%"class.std::locale" = type { ptr }

@_ZStL8__ioinit = internal global %"class.std::ios_base::Init" zeroinitializer, align 1
@__dso_handle = external hidden global i8
@_ZSt4cout = external global %"class.std::basic_ostream", align 8
@.str = private unnamed_addr constant [9 x i8] c"hi here!\00", align 1
@.str.1 = private unnamed_addr constant [9 x i8] c"io enter\00", align 1
@.str.2 = private unnamed_addr constant [8 x i8] c"io exit\00", align 1

declare void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr

; Function Attrs: nounwind
declare void @_ZNSt8ios_base4InitD1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr

; Function Attrs: nounwind
declare i32 @__cxa_atexit(ptr, ptr, ptr)

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local void @_Z5ioFn1v() {
; USE-LABEL: @_Z5ioFn1v() {
; USE: call void @io_enter()
  %1 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str)
  %2 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZNSolsEPFRSoS_E(ptr noundef nonnull align 8 dereferenceable(8) %1, ptr noundef @_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_)
; USE: call void @io_exit()
  ret void
}

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8), ptr noundef)

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZNSolsEPFRSoS_E(ptr noundef nonnull align 8 dereferenceable(8), ptr noundef)

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_(ptr noundef nonnull align 8 dereferenceable(8))

define dso_local noundef i32 @main() {
; USE-LABEL: @main() {
; USE: call void @io_enter()
  %1 = load ptr, ptr @_ZSt4cout, align 8
  %2 = getelementptr i8, ptr %1, i64 -24
  %3 = load i64, ptr %2, align 8
  %4 = getelementptr inbounds i8, ptr @_ZSt4cout, i64 %3
  %5 = call noundef ptr @_ZNKSt9basic_iosIcSt11char_traitsIcEE5rdbufEv(ptr noundef nonnull align 8 dereferenceable(264) %4)
  call void @_Z5ioFn1v()
; USE: call void @io_exit()
  ret i32 0
}

declare noundef ptr @_ZNKSt9basic_iosIcSt11char_traitsIcEE5rdbufEv(ptr noundef nonnull align 8 dereferenceable(264))

define dso_local void @io_enter() {
; USE-LABEL: @io_enter() {
; USE-NOT: call void @io_enter()
  %1 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.1)
  ret void
}

define dso_local void @io_exit() {
; USE-LABEL: @io_exit() {
; USE-NOT: call void @io_enter()
  %1 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.2)
  ret void
}
