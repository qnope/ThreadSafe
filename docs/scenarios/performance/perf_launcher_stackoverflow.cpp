#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace {

// macOS gives a non-main std::thread a 512 KiB stack by default.
// The payload itself lives on the heap; only the *parameter* copy is on the stack.
struct big_payload {
    std::array<char, 384 * 1024> storage{};
};

std::atomic<long long> sink{0};

void consume(big_payload payload) {
    sink.fetch_add(payload.storage[0], std::memory_order_relaxed);
}

}

int main(int argument_count, char** arguments) {
    const bool use_launcher = argument_count > 1 && arguments[1][0] == 'l';
    std::printf("default non-main thread stack: 512 KiB\n");
    std::printf("argument size: %zu bytes (lives on the heap)\n",
                sizeof(big_payload));
    std::printf("mode: %s\n", use_launcher ? "launch_task (by-value parameter)"
                                           : "emplace_back (forwarded)");
    std::fflush(stdout);

    std::unique_ptr<big_payload> source = std::make_unique<big_payload>();
    source->storage[0] = 3;

    std::jthread outer{[use_launcher, &source] {
        const big_payload& argument = *source;
        if (use_launcher) {
            threadsafe::asynchronous_task_launcher launcher;
            launcher.launch_task(consume, argument);
        } else {
            std::vector<std::jthread> threads;
            threads.emplace_back(consume, argument);
        }
        std::printf("  inner spawn SURVIVED\n");
        std::fflush(stdout);
    }};
    outer.join();
    std::printf("done, sink = %lld\n", sink.load());
}
