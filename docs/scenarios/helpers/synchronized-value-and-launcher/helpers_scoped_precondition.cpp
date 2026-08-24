#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <functional>

std::atomic<int>* registered_counter = nullptr;

// Capture-free, empty, defaulted members: fully sendable by the library's rules.
struct Registrar {
    void operator()(std::atomic<int>& counter) const {
        registered_counter = &counter;
    }
};

static_assert(threadsafe::is_sendable_v<Registrar>);
static_assert(threadsafe::launchable_scoped_task<
                  Registrar, std::reference_wrapper<std::atomic<int>>>,
              "launch_scoped_task accepts it: F is sendable, the arg is a "
              "reference to a synchronizable object");

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    {
        std::atomic<int> request_count{0};
        launcher.launch_scoped_task(Registrar{}, std::ref(request_count));
        std::printf("scoped task joined, request_count = %d\n",
                    request_count.load());
    }

    std::printf("later: %d\n", registered_counter->fetch_add(1) + 1);
}
