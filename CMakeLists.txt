cmake_minimum_required(VERSION 4.0)
project(ThreadSafe VERSION 0.1.0 LANGUAGES CXX)

add_library(threadsafe INTERFACE)
add_library(ThreadSafe::threadsafe ALIAS threadsafe)

target_include_directories(threadsafe INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(threadsafe INTERFACE cxx_std_26)
target_compile_options(threadsafe INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>)

option(THREADSAFE_BUILD_TESTS "Build compile-time tests" ${PROJECT_IS_TOP_LEVEL})
if(THREADSAFE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
