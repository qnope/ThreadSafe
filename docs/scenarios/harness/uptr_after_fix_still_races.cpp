// Same program as the repro, but using lock() (exclusive) instead of
// lock_shared(). Under the PROPOSED FIX this still compiles -- the race with
// the external writer is untouched, because the writer never takes the mutex.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

namespace {
struct NoopDeleter {
    void operator()(const int *) const noexcept {}
};
struct BorrowedReading {
    std::unique_ptr<const int, NoopDeleter> observed;
};
}

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;

static_assert(is_sendable_v<BorrowedReading>, "still sendable after the fix");
static_assert(is_lifetime_aware_v<BorrowedReading>, "still lifetime-aware after the fix");

int main() {
    int sensor_reading = 0;
    threadsafe::synchronized_value<BorrowedReading> shared_view{
        BorrowedReading{std::unique_ptr<const int, NoopDeleter>{&sensor_reading}}};

    std::thread writer{[&sensor_reading] {
        for (int sample = 0; sample < 20000; ++sample)
            sensor_reading = sample;
    }};

    std::atomic<long long> sink{0};
    auto reader = [&shared_view, &sink] {
        long long accumulated = 0;
        for (int iteration = 0; iteration < 20000; ++iteration) {
            const auto reader_guard = shared_view.lock();
            accumulated += *reader_guard->observed;
        }
        sink.fetch_add(accumulated, std::memory_order_relaxed);
    };

    std::thread first_reader{reader};
    std::thread second_reader{reader};
    writer.join();
    first_reader.join();
    second_reader.join();
    std::printf("done, last reading %d, sink %lld\n", sensor_reading, sink.load());
}
