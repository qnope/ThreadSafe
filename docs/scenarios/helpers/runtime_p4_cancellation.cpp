// Program 4 -- FAN-OUT WITH CANCELLATION.
// asynchronous_task_launcher exposes no stop_source, no request_stop(), no
// join() and no wait(). The only way to stop the tasks is to destroy the
// launcher, and the only way to wait for them is the same.
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p4_cancellation.cpp -o p4 && ./p4
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

namespace {

constexpr int task_count = 8;
constexpr auto poll_granularity = std::chrono::milliseconds(50);

// A task that works in chunks and can only notice a stop between chunks --
// which is what every real task looks like.
struct injected_token_task {
    std::shared_ptr<std::atomic<int>> chunks_done;

    void operator()(std::stop_token token) const {
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(poll_granularity);
            ++*chunks_done;
        }
    }
};

struct own_source_task {
    std::stop_source source;
    std::shared_ptr<std::atomic<int>> chunks_done;

    void operator()() const {
        const std::stop_token token = source.get_token();
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(poll_granularity);
            ++*chunks_done;
        }
    }
};

double milliseconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
}

// A. There is no request_stop() on the launcher. The only stop_source that
//    reaches injected_token_task is the one std::jthread owns internally, and
//    the launcher never hands it out. So the destructor is the stop button --
//    and std::vector<std::jthread>'s destructor destroys elements one at a
//    time, each doing request_stop() *then* join().
double shutdown_through_destructor_only() {
    auto chunks_done = std::make_shared<std::atomic<int>>(0);
    std::chrono::steady_clock::time_point stop_wanted_at;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int task = 0; task < task_count; ++task)
            launcher.launch_task(injected_token_task{chunks_done});
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        stop_wanted_at = std::chrono::steady_clock::now();
    }
    return milliseconds_since(stop_wanted_at);
}

// B. Smuggle a stop_source in as an ordinary argument -- vocabulary.h states
//    is_sendable<std::stop_source> and is_lifetime_aware<std::stop_source>, so
//    the concept lets it through. Now one request_stop() reaches every task at
//    once, and the destructor only has to join.
double shutdown_through_own_stop_source() {
    auto chunks_done = std::make_shared<std::atomic<int>>(0);
    std::stop_source source;
    std::chrono::steady_clock::time_point stop_wanted_at;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int task = 0; task < task_count; ++task)
            launcher.launch_task(own_source_task{source, chunks_done});
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        stop_wanted_at = std::chrono::steady_clock::now();
        source.request_stop();
    }
    return milliseconds_since(stop_wanted_at);
}

// C. What the same thing costs with a bare std::vector<std::jthread> when the
//    caller broadcasts the stop before joining.
double shutdown_bare_jthread_broadcast() {
    std::atomic<int> chunks_done{0};
    std::chrono::steady_clock::time_point stop_wanted_at;
    {
        std::vector<std::jthread> threads;
        for (int task = 0; task < task_count; ++task)
            threads.emplace_back([&chunks_done](std::stop_token token) {
                while (!token.stop_requested()) {
                    std::this_thread::sleep_for(poll_granularity);
                    ++chunks_done;
                }
            });
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        stop_wanted_at = std::chrono::steady_clock::now();
        for (auto& thread : threads)
            thread.request_stop();
    }
    return milliseconds_since(stop_wanted_at);
}

}

// The launcher's surface: no way to stop, wait, or count the tasks.
template <class Launcher>
constexpr bool has_request_stop = requires(Launcher l) { l.request_stop(); };
template <class Launcher>
constexpr bool has_join = requires(Launcher l) { l.join(); };
template <class Launcher>
constexpr bool has_wait = requires(Launcher l) { l.wait(); };
template <class Launcher>
constexpr bool has_stop_source = requires(Launcher l) { l.get_stop_source(); };
template <class Launcher>
constexpr bool has_size = requires(Launcher l) { l.size(); };

using launcher = threadsafe::asynchronous_task_launcher;
static_assert(!has_request_stop<launcher> && !has_join<launcher>
                  && !has_wait<launcher> && !has_stop_source<launcher>
                  && !has_size<launcher>,
              "the whole public surface is launch_task / launch_scoped_task");
static_assert(threadsafe::is_sendable_v<std::stop_source>
                  && threadsafe::is_lifetime_aware_v<std::stop_source>,
              "which is the loophole B uses");

int main() {
    std::printf("%d tasks, each notices a stop only every %lld ms\n\n",
                task_count, (long long)poll_granularity.count());
    std::printf("A. destructor only (no request_stop on the launcher) : %7.1f ms\n",
                shutdown_through_destructor_only());
    std::printf("B. own std::stop_source smuggled in as an argument   : %7.1f ms\n",
                shutdown_through_own_stop_source());
    std::printf("C. bare std::jthread, request_stop broadcast first   : %7.1f ms\n",
                shutdown_bare_jthread_broadcast());
}
