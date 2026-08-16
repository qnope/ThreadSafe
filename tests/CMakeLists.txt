# Compile-time test suite: building this target IS running the tests.
# OBJECT library so no main() is needed and the TUs are part of ALL.
add_library(threadsafe_tests OBJECT
    test_synchronizable.cpp
    test_sendable.cpp
    test_containers.cpp
    test_smart_pointers.cpp
    test_lifetime_aware.cpp
    test_asynchronous_task_launcher.cpp
    test_soundness_regressions.cpp
    test_include_isolation.cpp
    test_synchronized_value.cpp
    test_copy_on_write.cpp
)
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)
