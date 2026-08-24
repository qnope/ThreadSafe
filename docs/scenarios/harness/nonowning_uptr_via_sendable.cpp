// SAME borrowed-alias race, reached WITHOUT the const-unique_ptr rule.
// Only is_sendable<unique_ptr<T,D>> and is_lifetime_aware<unique_ptr<T,D>> are
// consulted -- both of which also assume "unique_ptr owns".
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>

namespace {

struct NoopDeleter {
    void operator()(int *) const noexcept {}
};

using Borrowed = std::unique_ptr<int, NoopDeleter>;

std::atomic<long long> sink{0};

void reader_task(Borrowed observed) {
    long long accumulated = 0;
    for (int iteration = 0; iteration < 200000; ++iteration)
        accumulated += *observed;
    sink.fetch_add(accumulated, std::memory_order_relaxed);
}

}

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;

static_assert(is_sendable_v<Borrowed>, "library says a non-owning unique_ptr may be sent");
static_assert(is_lifetime_aware_v<Borrowed>, "library says a non-owning unique_ptr keeps its referent alive");
static_assert(threadsafe::launchable_task<void (*)(Borrowed), Borrowed>,
              "launch_task accepts the borrowed alias");

int main() {
    int sensor_reading = 0;
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(&reader_task, Borrowed{&sensor_reading});
        launcher.launch_task(&reader_task, Borrowed{&sensor_reading});
        for (int sample = 0; sample < 200000; ++sample)
            sensor_reading = sample;
    }
    std::printf("done, last reading %d, sink %lld\n", sensor_reading, sink.load());
}
