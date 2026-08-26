#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <string>
#include <vector>

// A per-thread arena: the classic way to make allocation cheap and lock-free.
thread_local std::vector<std::string> arena;

// The handle a user hands around. An aggregate of one fundamental type.
struct ArenaHandle {
    std::size_t index;
};

std::string& resolve(ArenaHandle handle) { return arena[handle.index]; }

static_assert(threadsafe::is_sendable_v<ArenaHandle>);
static_assert(threadsafe::is_lifetime_aware_v<ArenaHandle>);
static_assert(threadsafe::launchable_task<void (*)(ArenaHandle), ArenaHandle>,
              "the library's own launcher accepts it");

void worker(ArenaHandle handle) {
    std::printf("worker arena size = %zu, handle index = %zu\n",
                arena.size(), handle.index);
    std::printf("worker read: '%s'\n", resolve(handle).c_str());
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    arena.emplace_back("owned by the main thread");
    ArenaHandle handle{0};
    std::printf("main arena size = %zu, main read: '%s'\n",
                arena.size(), resolve(handle).c_str());

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&worker, handle);   // accepted: sendable + lifetime aware
}
