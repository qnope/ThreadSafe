// The read-heavy configuration, done the only way the library accepts without
// copying the map per thread: copy_on_write.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <map>
#include <memory>
#include <print>
#include <string>

using configuration = std::map<std::string, std::string>;

int main() {
    threadsafe::copy_on_write<configuration> settings(
        configuration{{"host", "localhost"}, {"port", "8080"}});
    auto hits = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int reader_index = 0; reader_index < 8; ++reader_index)
            launcher.launch_task(
                [](threadsafe::copy_on_write<configuration> readable,
                   std::shared_ptr<std::atomic<int>> counter) {
                    for (int step = 0; step < 10000; ++step)
                        if (readable->at("host") == "localhost")
                            counter->fetch_add(1, std::memory_order_relaxed);
                },
                settings, hits);
    }

    std::println("hits = {}", hits->load());
}
