// A callable that is synchronizable but not movable — exactly the case the
// launcher's own message ("share it with std::ref instead") is written for.
#include <threadsafe/threadsafe.h>

#include <atomic>

struct counting_worker {
    std::atomic<int> calls{0};
    void operator()() const {}
};

template <>
struct threadsafe::is_synchronizable<counting_worker> : std::true_type {};

int main() {
    counting_worker worker;
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(worker);
}
