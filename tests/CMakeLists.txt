add_library(threadsafe_tests OBJECT
    test_synchronizable.cpp
    test_sendable.cpp
    test_containers.cpp
    test_smart_pointers.cpp
    test_lifetime_aware.cpp
    test_asynchronous_task_launcher.cpp
    test_soundness_regressions.cpp
    test_deferred_specialization.cpp
    test_synchronized_value.cpp
    test_copy_on_write.cpp
    test_diagnostics.cpp
)
target_link_libraries(threadsafe_tests PRIVATE ThreadSafe::threadsafe)
