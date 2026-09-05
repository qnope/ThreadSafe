cmake_minimum_required(VERSION 4.0)
project(ThreadSafe VERSION 0.1.0 LANGUAGES CXX)

add_library(threadsafe INTERFACE)
add_library(ThreadSafe::threadsafe ALIAS threadsafe)

set(THREADSAFE_HEADERS
    include/threadsafe/threadsafe.h
    include/threadsafe/details/allowed_std_wrappers.h
    include/threadsafe/details/asynchronous_task_launcher.h
    include/threadsafe/details/copy_on_write.h
    include/threadsafe/details/lifetime_aware.h
    include/threadsafe/details/sendable.h
    include/threadsafe/details/smart_pointers.h
    include/threadsafe/details/synchronizable.h
    include/threadsafe/details/synchronizable_base.h
    include/threadsafe/details/synchronized_value.h
    include/threadsafe/details/utils.h
    include/threadsafe/details/vocabulary.h)

target_sources(threadsafe
    PRIVATE ${THREADSAFE_HEADERS}
    INTERFACE FILE_SET HEADERS BASE_DIRS include FILES ${THREADSAFE_HEADERS})

target_compile_features(threadsafe INTERFACE cxx_std_26)
target_compile_options(threadsafe INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>)

option(THREADSAFE_BUILD_TESTS "Build compile-time tests" ${PROJECT_IS_TOP_LEVEL})
if(THREADSAFE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
