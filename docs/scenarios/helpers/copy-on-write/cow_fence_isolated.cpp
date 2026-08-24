// Isolates the acquire fence.  Main waits (on a RELAXED flag, which creates no
// happens-before of its own) until the worker has dropped its handle, so
// as_mutable() is guaranteed to take the in-place branch.  The only edge from
// the worker's reads to main's writes is:
//   worker's non-last release decrement (acq_rel RMW)
//     -> main's RELAXED use_count() load reading 1
//        -> main's std::atomic_thread_fence(memory_order_acquire).
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;

namespace {
struct Payload {
    std::vector<int> counters;
};

std::atomic<bool> handle_dropped{false};
std::atomic<bool> may_finish{false};
std::atomic<long long> observed_sum{0};
}

int main() {
    copy_on_write<Payload> document{Payload{std::vector<int>(256, 7)}};
    copy_on_write<Payload> worker_handle = document;

    std::thread worker([handle = std::move(worker_handle)]() mutable {
        long long local_sum = 0;
        for (int value : handle->counters)
            local_sum += value;
        observed_sum.store(local_sum, std::memory_order_relaxed);
        handle = copy_on_write<Payload>{Payload{}};
        handle_dropped.store(true, std::memory_order_relaxed);
        while (!may_finish.load(std::memory_order_relaxed))
            std::this_thread::yield();
    });

    while (!handle_dropped.load(std::memory_order_relaxed))
        std::this_thread::yield();

    const Payload* block_before = document.operator->();
    Payload& writable = document.as_mutable();
    std::printf("in_place=%d\n", &writable == block_before);
    for (int& counter : writable.counters)
        counter = 99;

    std::printf("sum=%lld first=%d\n",
                observed_sum.load(std::memory_order_relaxed),
                document->counters[0]);
    may_finish.store(true, std::memory_order_relaxed);
    worker.join();
    return 0;
}
