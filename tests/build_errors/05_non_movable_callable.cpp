#include <threadsafe/threadsafe.h>

struct Pinned {
    Pinned() = default;
    Pinned(const Pinned &) = delete;
    Pinned(Pinned &&) = delete;
    void operator()() const {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(Pinned{});
}
