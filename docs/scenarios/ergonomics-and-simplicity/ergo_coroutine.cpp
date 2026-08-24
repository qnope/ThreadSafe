#include <threadsafe/threadsafe.h>

#include <coroutine>
#include <generator>
#include <print>

std::generator<int> counted(int upper_bound) {
    for (int value = 0; value < upper_bound; ++value)
        co_yield value;
}

struct simple_task {
    struct promise_type {
        simple_task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

template <class T>
void report(const char *name) {
    std::println("{:34} send={:5} life={:5} sync={:5} const-sync={:5}", name,
                 is_sendable_v<T>, is_lifetime_aware_v<T>,
                 is_synchronizable_v<T>, is_synchronizable_v<const T>);
}

int main() {
    report<std::coroutine_handle<>>("std::coroutine_handle<>");
    report<std::generator<int>>("std::generator<int>");
    report<simple_task>("simple_task (empty handle-less)");
    report<std::suspend_never>("std::suspend_never");
}
