// Zero-overhead probe: the hand-written std::jthread path, same shape.
#include <thread>
#include <vector>

void worker(int index, double weight);

struct plain_launcher {
    std::vector<std::jthread> threads_;
};

void run(plain_launcher& launcher, int index, double weight) {
    launcher.threads_.emplace_back(worker, index, weight);
}
