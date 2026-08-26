#include <threadsafe/threadsafe.h>
#include <print>
#include <vector>

int main() {
    auto results = threadsafe::synchronized_value<std::vector<int>>::make();
    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_task(
        [](std::shared_ptr<threadsafe::synchronized_value<std::vector<int>>> sink) {
            sink->with([](std::vector<int>& values) { values.push_back(42); });
        },
        results);

    launcher.wait_all();
    std::println("size={} first={}", results->with_shared([](const std::vector<int>& v) { return v.size(); }),
                 results->with_shared([](const std::vector<int>& v) { return v.front(); }));
}
