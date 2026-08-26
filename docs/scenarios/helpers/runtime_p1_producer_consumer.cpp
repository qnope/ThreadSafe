// Program 1 -- PRODUCER / CONSUMER through synchronized_value<std::deque<Job>>.
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p1_producer_consumer.cpp -o p1 && ./p1
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <utility>

namespace {

struct Job {
    int identifier;
    std::string payload;
};

using job_queue = threadsafe::synchronized_value<std::deque<Job>>;
using consumer_tally = threadsafe::synchronized_value<std::uint64_t>;

// The launcher refuses a capturing lambda, so every task is a named struct whose
// members are exactly the captures -- and every member has to answer
// is_sendable and is_lifetime_aware on its own.
struct producer_task {
    std::shared_ptr<job_queue> queue;
    std::shared_ptr<std::atomic<bool>> production_finished;
    int job_count;

    void operator()() const {
        for (int identifier = 0; identifier < job_count; ++identifier) {
            auto queue_guard = queue->lock();
            queue_guard->push_back(
                Job{identifier, "payload-" + std::to_string(identifier)});
        }
        production_finished->store(true, std::memory_order_release);
    }
};

struct consumer_task {
    std::shared_ptr<job_queue> queue;
    std::shared_ptr<std::atomic<bool>> production_finished;
    std::shared_ptr<consumer_tally> consumed_count;
    std::shared_ptr<consumer_tally> consumed_identifier_sum;

    void operator()() const {
        std::uint64_t local_count = 0;
        std::uint64_t local_sum = 0;
        for (;;) {
            Job job{};
            bool got_one = false;
            {
                auto queue_guard = queue->lock();
                if (!queue_guard->empty()) {
                    job = std::move(queue_guard->front());
                    queue_guard->pop_front();
                    got_one = true;
                }
            }
            if (got_one) {
                ++local_count;
                local_sum += static_cast<std::uint64_t>(job.identifier)
                           + job.payload.size();
                continue;
            }
            if (production_finished->load(std::memory_order_acquire)) {
                auto queue_guard = queue->lock();
                if (queue_guard->empty())
                    break;
            }
        }
        // `*consumed_count->lock() += local_count;` is rejected: the rvalue
        // overloads of operator* / operator-> are deleted, although the
        // temporary guard would live to the end of the full-expression.
        {
            auto count_guard = consumed_count->lock();
            *count_guard += local_count;
        }
        {
            auto sum_guard = consumed_identifier_sum->lock();
            *sum_guard += local_sum;
        }
    }
};

}

int main() {
    constexpr int job_count = 100'000;
    constexpr int consumer_count = 3;

    auto queue = job_queue::make();
    auto production_finished = std::make_shared<std::atomic<bool>>(false);
    auto consumed_count = consumer_tally::make(0);
    auto consumed_identifier_sum = consumer_tally::make(0);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int consumer = 0; consumer < consumer_count; ++consumer)
            launcher.launch_task(consumer_task{queue, production_finished,
                                               consumed_count,
                                               consumed_identifier_sum});
        launcher.launch_task(
            producer_task{queue, production_finished, job_count});
        // No join()/wait() on the launcher: leaving this scope is the only way
        // to wait, and it is also the only way to stop the tasks.
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;

    std::uint64_t expected_sum = 0;
    for (int identifier = 0; identifier < job_count; ++identifier)
        expected_sum += static_cast<std::uint64_t>(identifier)
                      + ("payload-" + std::to_string(identifier)).size();

    const auto count_guard = consumed_count->lock_shared();
    const std::uint64_t observed_count = *count_guard;
    const auto sum_guard = consumed_identifier_sum->lock_shared();
    const std::uint64_t observed_sum = *sum_guard;
    const auto queue_guard = queue->lock_shared();
    const std::size_t leftover = queue_guard->size();

    std::printf("jobs produced        : %d\n", job_count);
    std::printf("jobs consumed        : %llu  (%s)\n",
                static_cast<unsigned long long>(observed_count),
                observed_count == job_count ? "exact" : "LOST OR DUPLICATED");
    std::printf("checksum consumed    : %llu\n",
                static_cast<unsigned long long>(observed_sum));
    std::printf("checksum expected    : %llu  (%s)\n",
                static_cast<unsigned long long>(expected_sum),
                observed_sum == expected_sum ? "match" : "MISMATCH");
    std::printf("queue left over      : %zu\n", leftover);
    std::printf("mutex selected       : %s\n",
                std::is_same_v<job_queue::mutex, std::shared_mutex>
                    ? "std::shared_mutex"
                    : "std::mutex");
    std::printf("wall clock           : %.1f ms\n",
                std::chrono::duration<double, std::milli>(elapsed).count());
    return observed_count == job_count && observed_sum == expected_sum ? 0 : 1;
}
