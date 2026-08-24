// Producer/consumer, the textbook way: a queue behind a mutex plus a condition
// variable so the consumer sleeps instead of spinning.
#include <threadsafe/threadsafe.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <print>

struct job_queue {
    std::deque<int> pending_jobs;
    bool closed = false;
};

int main() {
    auto queue = threadsafe::synchronized_value<job_queue>::make();
    std::condition_variable job_available;

    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_task(
        [](std::shared_ptr<threadsafe::synchronized_value<job_queue>> shared_queue) {
            for (int job_index = 0; job_index < 8; ++job_index) {
                auto queue_guard = shared_queue->lock();
                queue_guard->pending_jobs.push_back(job_index);
            }
        },
        queue);

    {
        auto queue_guard = queue->lock();
        job_available.wait(queue_guard, [&] { return !queue_guard->pending_jobs.empty(); });
        std::println("first job = {}", queue_guard->pending_jobs.front());
    }
}
