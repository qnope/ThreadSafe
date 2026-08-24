#include <threadsafe/threadsafe.h>
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>
namespace { struct Sink { std::atomic<int> n{0}; }; }
template <> struct threadsafe::is_synchronizable<Sink> : std::true_type {};
int main() {
    Sink sink;
    threadsafe::asynchronous_task_launcher launcher;
    auto start = std::chrono::steady_clock::now();
    for (int task = 0; task < 4; ++task)
        launcher.launch_scoped_task(
            [](Sink& s) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                s.n.fetch_add(1);
            },
            std::ref(sink));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("four 200ms scoped tasks took %lldms (parallel would be ~200)\n",
                (long long)elapsed);
}
