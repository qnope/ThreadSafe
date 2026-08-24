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
        std::_Exit(0);
    });
    watchdog.detach();

    {
        threadsafe::asynchronous_task_launcher launcher;

        launcher.launch_task(
            [](std::shared_ptr<sync_int> first, std::shared_ptr<sync_int> second) {
                for (int step = 0; step < 100000; ++step) {
                    auto first_guard = first->lock();
                    std::this_thread::yield();
                    auto second_guard = second->lock();
                    *first_guard += 1;
                    *second_guard += 1;
                }
                std::printf("left-then-right worker finished\n");
            },
            left, right);

        launcher.launch_task(
            [](std::shared_ptr<sync_int> first, std::shared_ptr<sync_int> second) {
                for (int step = 0; step < 100000; ++step) {
                    auto first_guard = first->lock();
                    std::this_thread::yield();
                    auto second_guard = second->lock();
                    *first_guard += 1;
                    *second_guard += 1;
                }
                std::printf("right-then-left worker finished\n");
            },
            right, left);
    }

    std::printf("no deadlock this run\n");
}
