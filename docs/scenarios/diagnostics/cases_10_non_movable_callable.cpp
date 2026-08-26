#include <threadsafe/threadsafe.h>

struct NonMovableCallable {
    NonMovableCallable() = default;
    NonMovableCallable(const NonMovableCallable&) = delete;
    NonMovableCallable(NonMovableCallable&&) = delete;
    void operator()() const {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(NonMovableCallable{});
}
