#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <stop_token>

static_assert(threadsafe::is_synchronizable_v<std::stop_source>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<std::stop_source>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<std::stop_source>>);

void hammer(std::shared_ptr<std::stop_source> shared_source) {
    std::stop_source a, b;
    for (int i = 0; i < 5'000'000; ++i)
        *shared_source = (i & 1) ? a : b;
}

int main() {
    for (int round = 0; round < 40; ++round) {
        auto shared_source = std::make_shared<std::stop_source>();
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(&hammer, shared_source);
        launcher.launch_task(&hammer, shared_source);
    }
    std::printf("survived\n");
}
