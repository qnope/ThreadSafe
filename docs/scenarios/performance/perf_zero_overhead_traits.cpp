// Zero-overhead probe: the ThreadSafe launcher path.
#include <threadsafe/threadsafe.h>

void worker(int index, double weight);

void run(threadsafe::asynchronous_task_launcher& launcher, int index, double weight) {
    launcher.launch_task(worker, index, weight);
}
