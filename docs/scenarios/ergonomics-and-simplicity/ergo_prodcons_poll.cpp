// Producer/consumer as the library allows it: no condition variable is reachable
// through value_guard, so the consumer polls.
#include <threadsafe/threadsafe.h>

#include <deque>
#include <memory>
#include <optional>
#include <print>
#include <thread>

struct job_queue {
    std::deque<int> pending_jobs;
    bool producer_finished = false;
};

using shared_job_queue = std::shared_ptr<threadsafe::synchronized_value<job_queue>>;

int main() {
    auto queue = threadsafe::synchronized_value<job_queue>::make();
    auto consumed_total = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;

        launcher.launch_task(
            [](shared_job_queue shared_queue) {
                for (int job_index = 1; job_index <= 100; ++job_index) {
                    auto queue_guard = shared_queue->lock();
                    queue_guard->pending_jobs.push_back(job_index);
                }
                auto queue_guard = shared_queue->lock();
                queue_guard->producer_finished = true;
            },
            queue);

        launcher.launch_task(
            [](shared_job_queue shared_queue,
               std::shared_ptr<std::atomic<int>> total) {
                for (;;) {
                    std::optional<int> job;
                    bool finished = false;
                    {
                        auto queue_guard = shared_queue->lock();
                        if (!queue_guard->pending_jobs.empty()) {
                            job = queue_guard->pending_jobs.front();
                            queue_guard->pending_jobs.pop_front();
                        } else {
                            finished = queue_guard->producer_finished;
                        }
                    }
                    if (job)
                        total->fetch_add(*job, std::memory_order_relaxed);
                    else if (finished)
                        return;
                    else
                        std::this_thread::yield();
                }
            },
            queue, consumed_total);
    }

    std::println("sum of consumed jobs = {} (expected 5050)",
                 consumed_total->load());
}
