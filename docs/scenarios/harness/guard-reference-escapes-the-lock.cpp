#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>

using sync_int = threadsafe::synchronized_value<int>;

// Nothing exotic: a helper that reads the value the caller has locked.
// `operator*` on a const lvalue guard is the supported spelling.
int& peek_under_lock(const sync_int::guard& locked) { return *locked; }

int main() {
    auto shared_counter = sync_int::make(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 2; ++worker)
            launcher.launch_task(
                [](std::shared_ptr<sync_int> counter) {
                    for (int step = 0; step < 100000; ++step) {
                        // The guard is a temporary bound to a reference
                        // parameter: it dies at the semicolon, so the lock is
                        // already released when `escaped` is used.
                        int& escaped = peek_under_lock(counter->lock());
                        escaped += 1;
                    }
                },
                shared_counter);
    }

    auto final_guard = shared_counter->lock();
    std::printf("counter = %d (expected 200000)\n", *final_guard);
}
