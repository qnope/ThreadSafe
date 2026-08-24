#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace {
struct Memo {
    int key;
    mutable int cached;
};
}

using sync_vector = threadsafe::synchronized_value<std::vector<int>>;
using sync_memo = threadsafe::synchronized_value<Memo>;

static_assert(std::is_same_v<sync_vector::mutex, std::shared_mutex>);
static_assert(std::is_same_v<sync_memo::mutex, std::mutex>);

std::atomic<int> readers_inside{0};
std::atomic<int> peak_readers{0};

template <class SyncValue>
void measure(const char* label, std::shared_ptr<SyncValue> shared) {
    readers_inside = 0;
    peak_readers = 0;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task(
                [](std::shared_ptr<SyncValue> value) {
                    auto reading = value->lock_shared();
                    const int now = readers_inside.fetch_add(1) + 1;
                    int previous_peak = peak_readers.load();
                    while (previous_peak < now
                           && !peak_readers.compare_exchange_weak(previous_peak, now))
                        ;
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    readers_inside.fetch_sub(1);
                    (void)*reading;
                },
                shared);
    }
    std::printf("%-34s peak concurrent readers = %d / 4\n", label,
                peak_readers.load());
}

int main() {
    measure("vector<int> (shared_mutex)", sync_vector::make());
    measure("Memo, mutable member (mutex)", sync_memo::make(Memo{1, 2}));
}
