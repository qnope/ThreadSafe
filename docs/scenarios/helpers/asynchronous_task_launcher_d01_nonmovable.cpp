#include <threadsafe/threadsafe.h>

struct NonMovable {
    NonMovable() = default;
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
    void operator()() const {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(NonMovable{});
}
