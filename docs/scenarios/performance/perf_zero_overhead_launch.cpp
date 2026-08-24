#include <threadsafe/threadsafe.h>

#include <thread>
#include <vector>

void sink(int, double);

struct Work {
    void operator()(int a, double b) const { sink(a, b); }
};

// --- library path -----------------------------------------------------------
void via_launcher(threadsafe::asynchronous_task_launcher& launcher,
                  int first_argument, double second_argument) {
    launcher.launch_task(Work{}, first_argument, second_argument);
}

// --- hand written path ------------------------------------------------------
void via_raw_vector(std::vector<std::jthread>& threads,
                    int first_argument, double second_argument) {
    threads.emplace_back(Work{}, first_argument, second_argument);
}

// --- pure jthread (no vector) ----------------------------------------------
void via_raw_jthread(int first_argument, double second_argument) {
    std::jthread thread{Work{}, first_argument, second_argument};
    thread.detach();
}
