// Hand-extracted plain-C++ equivalent (no reflection) of the launch_task scenario.
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

thread_local std::vector<std::string> arena;

struct ArenaHandle { std::size_t index; };

std::string& resolve(ArenaHandle handle) { return arena[handle.index]; }

void worker(ArenaHandle handle) {
    std::printf("worker arena size = %zu\n", arena.size());
    std::printf("worker read: '%s'\n", resolve(handle).c_str());
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    arena.emplace_back("owned by the main thread");
    ArenaHandle handle{0};
    std::thread t{worker, handle};
    t.join();
}
