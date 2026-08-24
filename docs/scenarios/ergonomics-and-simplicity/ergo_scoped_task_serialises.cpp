// launch_scoped_task is the launcher's answer for "share a reference with a task
// and wait for it". Two such tasks, each sleeping 500 ms.
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <print>
#include <thread>

int main() {
    const auto started_at = std::chrono::steady_clock::now();

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(
        [] { std::this_thread::sleep_for(std::chrono::milliseconds{500}); });
    launcher.launch_scoped_task(
        [] { std::this_thread::sleep_for(std::chrono::milliseconds{500}); });

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    std::println("two 500 ms scoped tasks took {} ms", elapsed.count());
}
