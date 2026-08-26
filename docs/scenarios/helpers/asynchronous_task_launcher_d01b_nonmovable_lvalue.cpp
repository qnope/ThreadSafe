#include <threadsafe/threadsafe.h>
#include <utility>

struct NonMovable {
    NonMovable() = default;
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
    void operator()() const {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    NonMovable f;
    launcher.launch_task(std::move(f));
}
