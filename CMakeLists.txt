cmake_minimum_required(VERSION 3.20)
project(boundary)

add_compile_definitions(D_LINK_TIME_DUMPER=1)

# Set this to a valid LLVM installation dir
set(LT_LLVM_INSTALL_DIR "" CACHE PATH "LLVM installation directory")

# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)
if("${LLVM_VERSION_MAJOR}" VERSION_LESS 15)
    message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 15 or above")
endif()

# Boundary includes headers from LLVM - update the include paths accordingly
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
if (NOT "$ENV{LLVM_INCLUDE}" STREQUAL "")
    include_directories(SYSTEM "$ENV{LLVM_INCLUDE}")
endif ()

# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
set(CMAKE_C_STANDARD 17 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

set(PLUGIN_NAME Boundary)
add_library(${PLUGIN_NAME} SHARED boundary.cpp)
target_include_directories(${PLUGIN_NAME} PRIVATE "include")
target_link_libraries(${PLUGIN_NAME} "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set(SHED_NAME Shed)
add_library(${SHED_NAME} SHARED shed.cpp)
target_include_directories(${SHED_NAME} PRIVATE "include")

add_library(${SHED_NAME}C SHARED shed.c)
target_include_directories(${SHED_NAME}C PRIVATE "include")

set(PERF_TEST_NAME PerfTest)
add_executable(${PERF_TEST_NAME} perf_test.c)
target_include_directories(${PERF_TEST_NAME} PRIVATE "include")
target_link_libraries(${PERF_TEST_NAME} m)
