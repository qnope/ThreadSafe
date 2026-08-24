// Both launch paths in ONE translation unit, so GCC's outlining heuristics are
// identical for both. run_traits() goes through launch_task; run_plain() is the
// hand-written jthread emplace.
#include <threadsafe/threadsafe.h>
#include <thread>
#include <vector>

void worker(int index, double weight);

struct plain_launcher {
    std::vector<std::jthread> threads_;
};

void run_traits(threadsafe::asynchronous_task_launcher& launcher, int index, double weight) {
    launcher.launch_task(worker, index, weight);
}

void run_plain(plain_launcher& launcher, int index, double weight) {
    launcher.threads_.emplace_back(worker, index, weight);
}
