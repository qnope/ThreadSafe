#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

using sync_int = threadsafe::synchronized_value<int>;

int main() {
    auto left = sync_int::make(0);
    auto right = sync_int::make(0);

    std::thread watchdog([] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::printf("WATCHDOG: still blocked after 2s -> deadlock\n");
        std::fflush(stdout);
        std::_Exit(1);
    });
    watchdog.detach();

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 2; ++worker)
            launcher.launch_task(
                [](std::shared_ptr<sync_int> first, std::shared_ptr<sync_int> second) {
                    for (int step = 0; step < 100000; ++step)
                        threadsafe::with_all_locked(
                            [](int& a, int& b) { a += 1; b += 1; },
                            *first, *second);
                },
                worker == 0 ? left : right,
                worker == 0 ? right : left);
    }

    auto left_guard = left->lock();
    auto right_guard = right->lock();
    std::printf("no deadlock: left = %d, right = %d\n", *left_guard, *right_guard);
}
