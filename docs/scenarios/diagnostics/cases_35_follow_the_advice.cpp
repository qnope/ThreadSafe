#include <threadsafe/threadsafe.h>
#include <functional>

struct NonMovableCallable {
    NonMovableCallable() = default;
    NonMovableCallable(const NonMovableCallable&) = delete;
    NonMovableCallable(NonMovableCallable&&) = delete;
    void operator()() const {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    NonMovableCallable callable;
    // The library just told us: "share it with std::ref instead".
    launcher.launch_task(std::ref(callable));
}
