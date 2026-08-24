// is_synchronizable<const unique_ptr<T,D>> trusts the pointee's own const
// ("owned storage: the element keeps its own cv through get()"). A unique_ptr
// whose deleter does not delete -- the standard idiom for a borrowed handle --
// breaks that assumption while every trait still answers yes.

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
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<BorrowedReading>);
static_assert(is_lifetime_aware_v<BorrowedReading>);
static_assert(is_synchronizable_v<const BorrowedReading>,
              "the library calls this readable from several threads at once");
static_assert(std::is_same_v<threadsafe::synchronized_value<BorrowedReading>::mutex,
                             std::shared_mutex>);

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
            const auto reader_guard = shared_view.lock_shared();
            accumulated += *reader_guard->observed;
        }
        sink.fetch_add(accumulated, std::memory_order_relaxed);
    };

    std::thread first_reader{reader};
    std::thread second_reader{reader};
    writer.join();
    first_reader.join();
    second_reader.join();

    std::printf("done, last reading %d, sink %lld\n", sensor_reading,
                sink.load());
    return 0;
}
