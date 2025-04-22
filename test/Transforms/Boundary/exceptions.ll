; Test instrumentation of LLVM exception handling mechanism.
; RUN: opt < %s -load-pass-plugin libBoundary.so -passes="instrument<boundary>" --load-function-profiles=stdlib++-io-profile.txt --intrinsic-io-enter="io_enter" --intrinsic-io-exit="io_exit" --complexity-threshold=0 -S | FileCheck %s --check-prefix=USE

%"class.std::ios_base::Init" = type { i8 }
%"class.std::basic_ostream" = type { ptr, %"class.std::basic_ios" }
%"class.std::basic_ios" = type { %"class.std::ios_base", ptr, i8, i8, ptr, ptr, ptr, ptr }
%"class.std::ios_base" = type { ptr, i64, i64, i32, i32, i32, ptr, %"struct.std::ios_base::_Words", [8 x %"struct.std::ios_base::_Words"], i32, ptr, %"class.std::locale" }
%"struct.std::ios_base::_Words" = type { ptr, i64 }
%"class.std::locale" = type { ptr }
%struct.A = type { i8 }

$__clang_call_terminate = comdat any

$_ZN1AD2Ev = comdat any

@_ZStL8__ioinit = internal global %"class.std::ios_base::Init" zeroinitializer, align 1
@__dso_handle = external hidden global i8
@_ZSt4cout = external global %"class.std::basic_ostream", align 8
@.str = private unnamed_addr constant [10 x i8] c"throwing\0A\00", align 1
@_ZTIi = external constant ptr
@_ZTIf = external constant ptr
@.str.1 = private unnamed_addr constant [15 x i8] c"no exceptions\0A\00", align 1
@.str.2 = private unnamed_addr constant [21 x i8] c"non-float exception\0A\00", align 1
@.str.3 = private unnamed_addr constant [17 x i8] c"float exception\0A\00", align 1
@.str.4 = private unnamed_addr constant [4 x i8] c"~A\0A\00", align 1
@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @_GLOBAL__sub_I_tmp6.cpp, ptr null }]

; Function Attrs: noinline uwtable
define internal void @__cxx_global_var_init() #0 section ".text.startup" {
  call void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1) @_ZStL8__ioinit)
  %1 = call i32 @__cxa_atexit(ptr @_ZNSt8ios_base4InitD1Ev, ptr @_ZStL8__ioinit, ptr @__dso_handle) #3
  ret void
}

declare void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr #1

; Function Attrs: nounwind
declare void @_ZNSt8ios_base4InitD1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr #2

; Function Attrs: nounwind
declare i32 @__cxa_atexit(ptr, ptr, ptr) #3

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local void @_Z8throwingi(i32 noundef %0) #4 {
; USE-LABEL: @_Z8throwingi(i32 noundef %0) #4 {
; USE: call void @io_enter()
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str)
  %4 = load i32, ptr %2, align 4
  %5 = icmp eq i32 %4, 1
  br i1 %5, label %6, label %8

6:                                                ; preds = %1
  %7 = call ptr @__cxa_allocate_exception(i64 4) #3
  store i32 1, ptr %7, align 16
; USE-LABEL: store i32 1, ptr %7, align 16
; USE: call void @io_exit()
  call void @__cxa_throw(ptr %7, ptr @_ZTIi, ptr null) #9
  unreachable

8:                                                ; preds = %1
  %9 = load i32, ptr %2, align 4
  %10 = icmp eq i32 %9, 2
  br i1 %10, label %11, label %13

11:                                               ; preds = %8
  %12 = call ptr @__cxa_allocate_exception(i64 4) #3
  store float 1.000000e+00, ptr %12, align 16
; USE-LABEL: store float 1.000000e+00, ptr %12, align 16
; USE: call void @io_exit()
  call void @__cxa_throw(ptr %12, ptr @_ZTIf, ptr null) #9
  unreachable

13:                                               ; preds = %8
  br label %14

14:                                               ; preds = %13
  %15 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.1)

; USE-LABEL: call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.1)
; USE: call void @io_exit()
  ret void
}

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8), ptr noundef) #1

declare ptr @__cxa_allocate_exception(i64)

declare void @__cxa_throw(ptr, ptr, ptr)

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #5 personality ptr @__gxx_personality_v0 {
; USE-LABEL: @main() #5 personality ptr @__gxx_personality_v0 {
; USE: call void @io_enter()
  %1 = alloca i32, align 4
  %2 = alloca %struct.A, align 1
  %3 = alloca ptr, align 8
  %4 = alloca i32, align 4
  %5 = alloca float, align 4
  store i32 0, ptr %1, align 4
  invoke void @_Z8throwingi(i32 noundef 1)
          to label %6 unwind label %7

6:                                                ; preds = %0
  br label %21

7:                                                ; preds = %0
  %8 = landingpad { ptr, i32 }
          catch ptr @_ZTIf
          catch ptr null
  %9 = extractvalue { ptr, i32 } %8, 0
  store ptr %9, ptr %3, align 8
  %10 = extractvalue { ptr, i32 } %8, 1
  store i32 %10, ptr %4, align 4
  br label %11

11:                                               ; preds = %7
  %12 = load i32, ptr %4, align 4
  %13 = call i32 @llvm.eh.typeid.for(ptr @_ZTIf) #3
  %14 = icmp eq i32 %12, %13
  br i1 %14, label %15, label %23

15:                                               ; preds = %11
  %16 = load ptr, ptr %3, align 8
  %17 = call ptr @__cxa_begin_catch(ptr %16) #3
  %18 = load float, ptr %17, align 4
  store float %18, ptr %5, align 4
  %19 = invoke noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.3)
          to label %20 unwind label %38

20:                                               ; preds = %15
  call void @__cxa_end_catch() #3
  br label %21

21:                                               ; preds = %20, %28, %6
  call void @_ZN1AD2Ev(ptr noundef nonnull align 1 dereferenceable(1) %2) #3
  %22 = load i32, ptr %1, align 4
; USE-LABEL: %22 = load i32, ptr %1, align 4
; USE: call void @io_exit()
  ret i32 %22

23:                                               ; preds = %11
  %24 = load ptr, ptr %3, align 8
  %25 = call ptr @__cxa_begin_catch(ptr %24) #3
  %26 = invoke noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.2)
          to label %27 unwind label %29

27:                                               ; preds = %23
  invoke void @__cxa_end_catch()
          to label %28 unwind label %33

28:                                               ; preds = %27
  br label %21

29:                                               ; preds = %23
  %30 = landingpad { ptr, i32 }
          cleanup
  %31 = extractvalue { ptr, i32 } %30, 0
  store ptr %31, ptr %3, align 8
  %32 = extractvalue { ptr, i32 } %30, 1
  store i32 %32, ptr %4, align 4
  invoke void @__cxa_end_catch()
          to label %37 unwind label %48

33:                                               ; preds = %27
  %34 = landingpad { ptr, i32 }
          cleanup
  %35 = extractvalue { ptr, i32 } %34, 0
  store ptr %35, ptr %3, align 8
  %36 = extractvalue { ptr, i32 } %34, 1
  store i32 %36, ptr %4, align 4
  br label %42

37:                                               ; preds = %29
  br label %42

38:                                               ; preds = %15
  %39 = landingpad { ptr, i32 }
          cleanup
  %40 = extractvalue { ptr, i32 } %39, 0
  store ptr %40, ptr %3, align 8
  %41 = extractvalue { ptr, i32 } %39, 1
  store i32 %41, ptr %4, align 4
  call void @__cxa_end_catch() #3
  br label %42

42:                                               ; preds = %38, %37, %33
  call void @_ZN1AD2Ev(ptr noundef nonnull align 1 dereferenceable(1) %2) #3
  br label %43

43:                                               ; preds = %42
  %44 = load ptr, ptr %3, align 8
  %45 = load i32, ptr %4, align 4
  %46 = insertvalue { ptr, i32 } poison, ptr %44, 0
  %47 = insertvalue { ptr, i32 } %46, i32 %45, 1
; USE-LABEL: %47 = insertvalue { ptr, i32 } %46, i32 %45, 1
; USE: call void @io_exit()
  resume { ptr, i32 } %47

48:                                               ; preds = %29
  %49 = landingpad { ptr, i32 }
          catch ptr null
  %50 = extractvalue { ptr, i32 } %49, 0
; USE-LABEL: %50 = extractvalue { ptr, i32 } %49, 0
; USE: call void @io_exit()
  call void @__clang_call_terminate(ptr %50) #10
  unreachable
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind memory(none)
declare i32 @llvm.eh.typeid.for(ptr) #6

declare ptr @__cxa_begin_catch(ptr)

declare void @__cxa_end_catch()

; Function Attrs: noinline noreturn nounwind uwtable
define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #7 comdat {
  %2 = call ptr @__cxa_begin_catch(ptr %0) #3
; USE-LABEL: %2 = call ptr @__cxa_begin_catch(ptr %0) #3
; USE-NOT: call void @io_exit()
  call void @_ZSt9terminatev() #10
  unreachable
}

declare void @_ZSt9terminatev()

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr dso_local void @_ZN1AD2Ev(ptr noundef nonnull align 1 dereferenceable(1) %0) unnamed_addr #8 comdat align 2 personality ptr @__gxx_personality_v0 {
; USE-LABEL: @_ZN1AD2Ev(ptr noundef nonnull align 1 dereferenceable(1) %0) unnamed_addr #8 comdat align 2 personality ptr @__gxx_personality_v0 {
; USE: call void @io_enter()
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = invoke noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef @.str.4)
          to label %5 unwind label %6

5:                                                ; preds = %1
; USE: call void @io_exit()
  ret void

6:                                                ; preds = %1
  %7 = landingpad { ptr, i32 }
          catch ptr null
  %8 = extractvalue { ptr, i32 } %7, 0
; USE-LABEL: %8 = extractvalue { ptr, i32 } %7, 0
; USE: call void @io_exit()
  call void @__clang_call_terminate(ptr %8) #10
  unreachable
}

; Function Attrs: noinline uwtable
define internal void @_GLOBAL__sub_I_tmp6.cpp() #0 section ".text.startup" {
  call void @__cxx_global_var_init()
  ret void
}

declare void @io_enter()

declare void @io_exit()

attributes #0 = { noinline uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind }
attributes #4 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind memory(none) }
attributes #7 = { noinline noreturn nounwind uwtable "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { noreturn }
attributes #10 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 18.1.8 (++20240731024944+3b5b5c1ec4a3-1~exp1~20240731145000.144)"}
