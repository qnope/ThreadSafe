// CONTROL: two textbook, unambiguously CORRECT fence handoffs.
// If TSan reports these, TSan cannot model std::atomic_thread_fence, and any
// report it makes about copy_on_write's fence is a tool limitation.
#include <atomic>
#include <cstdio>
#include <thread>

static int payload = 0;
static std::atomic<int> flag{0};

// [atomics.fences]/2 -- fence-fence synchronization.
static void fence_fence() {
    payload = 0; flag.store(0, std::memory_order_relaxed);
    std::thread producer([] {
        payload = 42;
        std::atomic_thread_fence(std::memory_order_release);
        flag.store(1, std::memory_order_relaxed);
    });
    std::thread consumer([] {
        while (flag.load(std::memory_order_relaxed) != 1) {}
        std::atomic_thread_fence(std::memory_order_acquire);
        payload += 1;
    });
    producer.join(); consumer.join();
}

// [atomics.fences]/4 -- fence-atomic synchronization: a relaxed load that reads
// a release store, followed by an acquire fence. This is EXACTLY the shape
// copy_on_write::as_mutable() uses (relaxed use_count() load + acquire fence
// pairing with the release refcount decrement).
static void fence_atomic() {
    payload = 0; flag.store(0, std::memory_order_relaxed);
    std::thread producer([] {
        payload = 42;
        flag.store(1, std::memory_order_release);
    });
    std::thread consumer([] {
        while (flag.load(std::memory_order_relaxed) != 1) {}
        std::atomic_thread_fence(std::memory_order_acquire);
        payload += 1;
    });
    producer.join(); consumer.join();
}

int main(int argc, char** argv) {
    const bool run_fence_fence = argc < 2 || argv[1][0] == '2';
    for (int i = 0; i < 200; ++i) {
        if (run_fence_fence) fence_fence();
        else fence_atomic();
    }
    std::printf("done %d\n", payload);
}
