// Baseline for program 1: the same producer/consumer, hand written, so the only
// difference is which mutex protects the deque and whether a condition_variable
// is available.  build: g++-16 -std=c++26 -O2 -pthread p1b_baseline.cpp -o p1b
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
struct Job { int identifier; std::string payload; };

template <class Mutex>
struct spin_queue {
    Mutex mutex;
    std::deque<Job> jobs;
    std::atomic<bool> production_finished{false};
    std::atomic<std::uint64_t> consumed_count{0};
    std::atomic<std::uint64_t> consumed_sum{0};
};

template <class Mutex>
double run_spinning(int job_count, int consumer_count) {
    spin_queue<Mutex> shared;
    const auto started_at = std::chrono::steady_clock::now();
    std::vector<std::jthread> threads;
    for (int consumer = 0; consumer < consumer_count; ++consumer)
        threads.emplace_back([&shared] {
            std::uint64_t local_count = 0, local_sum = 0;
            for (;;) {
                Job job{};
                bool got_one = false;
                {
                    std::unique_lock lock{shared.mutex};
                    if (!shared.jobs.empty()) {
                        job = std::move(shared.jobs.front());
                        shared.jobs.pop_front();
                        got_one = true;
                    }
                }
                if (got_one) {
                    ++local_count;
                    local_sum += static_cast<std::uint64_t>(job.identifier)
                               + job.payload.size();
                    continue;
                }
                if (shared.production_finished.load(std::memory_order_acquire)) {
                    std::unique_lock lock{shared.mutex};
                    if (shared.jobs.empty()) break;
                }
            }
            shared.consumed_count += local_count;
            shared.consumed_sum += local_sum;
        });
    threads.emplace_back([&shared, job_count] {
        for (int identifier = 0; identifier < job_count; ++identifier) {
            std::unique_lock lock{shared.mutex};
            shared.jobs.push_back(
                Job{identifier, "payload-" + std::to_string(identifier)});
        }
        shared.production_finished.store(true, std::memory_order_release);
    });
    threads.clear();
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    std::printf("   consumed %llu\n",
                static_cast<unsigned long long>(shared.consumed_count.load()));
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

struct cv_queue {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Job> jobs;
    bool production_finished = false;
    std::atomic<std::uint64_t> consumed_count{0};
};

double run_condition_variable(int job_count, int consumer_count) {
    cv_queue shared;
    const auto started_at = std::chrono::steady_clock::now();
    std::vector<std::jthread> threads;
    for (int consumer = 0; consumer < consumer_count; ++consumer)
        threads.emplace_back([&shared] {
            std::uint64_t local_count = 0;
            for (;;) {
                std::unique_lock lock{shared.mutex};
                shared.ready.wait(lock, [&shared] {
                    return !shared.jobs.empty() || shared.production_finished;
                });
                if (shared.jobs.empty()) break;
                Job job = std::move(shared.jobs.front());
                shared.jobs.pop_front();
                lock.unlock();
                ++local_count;
            }
            shared.consumed_count += local_count;
        });
    threads.emplace_back([&shared, job_count] {
        for (int identifier = 0; identifier < job_count; ++identifier) {
            {
                std::unique_lock lock{shared.mutex};
                shared.jobs.push_back(
                    Job{identifier, "payload-" + std::to_string(identifier)});
            }
            shared.ready.notify_one();
        }
        {
            std::unique_lock lock{shared.mutex};
            shared.production_finished = true;
        }
        shared.ready.notify_all();
    });
    threads.clear();
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    std::printf("   consumed %llu\n",
                static_cast<unsigned long long>(shared.consumed_count.load()));
    return std::chrono::duration<double, std::milli>(elapsed).count();
}
}

int main() {
    constexpr int job_count = 100'000;
    constexpr int consumer_count = 3;
    std::printf("spin + shared_mutex (what synchronized_value picks): %.1f ms\n",
                run_spinning<std::shared_mutex>(job_count, consumer_count));
    std::printf("spin + mutex        (same code, plain mutex)       : %.1f ms\n",
                run_spinning<std::mutex>(job_count, consumer_count));
    std::printf("condition_variable  (impossible with the wrapper)  : %.1f ms\n",
                run_condition_variable(job_count, consumer_count));
}
