// Program 7 -- AN ACTUAL HANG.
// The classic two-account transfer. std::mutex has std::scoped_lock, which
// deadlock-avoids. synchronized_value keeps its mutex private and offers no
// multi-lock, so the only thing a user can write is lock-one-then-the-other --
// and two transfers in opposite directions deadlock.
//
// A watchdog thread aborts the process after 3 s of no progress so the program
// still terminates and prints what happened.
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p7_deadlock.cpp -o p7 && ./p7 ; echo "exit=$?"
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using account = threadsafe::synchronized_value<long long>;

constexpr int transfers = 200'000;

std::atomic<long long> progress{0};

struct transfer_task {
    std::shared_ptr<account> from;
    std::shared_ptr<account> to;

    void operator()() const {
        for (int step = 0; step < transfers; ++step) {
            // There is no threadsafe::lock(a, b) and no way to reach the two
            // mutexes, so std::scoped_lock is not available. Two locks, taken
            // one at a time, in the order the arguments came in.
            auto from_guard = from->lock();
            auto to_guard = to->lock();
            *from_guard -= 1;
            *to_guard += 1;
            ++progress;
        }
    }
};

// The mutex is private and there is no accessor, so none of these compile.
template <class Value>
constexpr bool exposes_mutex = requires(Value v) { v.get_mutex(); };
template <class Value>
constexpr bool exposes_native_handle = requires(Value v) { v.native_handle(); };
// synchronized_value has a member called lock(), so std::scoped_lock's
// constructor *declaration* accepts it -- the requires-expression below is
// true. Instantiating it is a different story; see p7b_scoped_lock.cpp.
template <class Value>
constexpr bool scoped_lock_looks_ok =
    requires(Value& a, Value& b) { std::scoped_lock{a, b}; };
template <class Value>
constexpr bool has_try_lock = requires(Value& v) { v.try_lock(); };
template <class Value>
constexpr bool has_unlock = requires(Value& v) { v.unlock(); };

}

static_assert(!exposes_mutex<account> && !exposes_native_handle<account>,
              "no way to hand the two mutexes to std::scoped_lock / std::lock");
static_assert(scoped_lock_looks_ok<account>,
              "and yet std::scoped_lock{a, b} passes overload resolution");
static_assert(!has_try_lock<account> && !has_unlock<account>,
              "although it is not a Lockable at all");

int main() {
    auto first = account::make(1'000'000LL);
    auto second = account::make(1'000'000LL);

    std::jthread watchdog{[](std::stop_token token) {
        long long previous = -1;
        int stalled_seconds = 0;
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const long long now = progress.load();
            stalled_seconds = (now == previous) ? stalled_seconds + 1 : 0;
            previous = now;
            if (stalled_seconds >= 3) {
                std::printf("DEADLOCK: no progress for 3 s, stuck after %lld "
                            "of %d transfers\n",
                            now, 2 * transfers);
                std::fflush(stdout);
                std::_Exit(2);
            }
        }
    }};

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(transfer_task{first, second});
        launcher.launch_task(transfer_task{second, first});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    watchdog.request_stop();

    const auto first_guard = first->lock_shared();
    const auto second_guard = second->lock_shared();
    std::printf("no deadlock this run: %lld + %lld = %lld in %.1f ms\n",
                *first_guard, *second_guard, *first_guard + *second_guard,
                std::chrono::duration<double, std::milli>(elapsed).count());
}
