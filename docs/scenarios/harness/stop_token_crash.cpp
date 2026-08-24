#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
#include <stop_token>
static_assert(threadsafe::is_synchronizable_v<std::stop_token>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<std::stop_token>>);
void hammer(std::shared_ptr<std::stop_token> shared) {
    std::stop_source a, b;
    std::stop_token ta = a.get_token(), tb = b.get_token();
    for (int i = 0; i < 5'000'000; ++i)
        *shared = (i & 1) ? ta : tb;
}
int main() {
    for (int round = 0; round < 40; ++round) {
        std::stop_source s;
        auto shared = std::make_shared<std::stop_token>(s.get_token());
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(&hammer, shared);
        launcher.launch_task(&hammer, shared);
    }
    std::printf("survived\n");
}
