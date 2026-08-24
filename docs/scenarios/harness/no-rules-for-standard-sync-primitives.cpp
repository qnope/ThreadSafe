#include <threadsafe/threadsafe.h>

#include <functional>
#include <latch>

void worker(std::latch& arrival) { arrival.count_down(); }

int main() {
    std::latch arrival{2};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(&worker, std::ref(arrival));
}
