#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
#include <stop_token>

// Idiomatic-looking pattern: a restartable worker pool. One thread resets the
// shared cancellation handle for the next batch, the other cancels the batch.
void resetter(std::shared_ptr<std::stop_source> shared_source) {
    for (int i = 0; i < 200000; ++i)
        *shared_source = std::stop_source{};
}
void canceller(std::shared_ptr<std::stop_source> shared_source) {
    for (int i = 0; i < 200000; ++i)
        shared_source->request_stop();
}
int main() {
    auto shared_source = std::make_shared<std::stop_source>();
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&resetter, shared_source);
    launcher.launch_task(&canceller, shared_source);
    std::printf("done\n");
}
