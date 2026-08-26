// Program 5, THE WRONG VERSION written as a named functor, because the launcher
// refuses capturing lambdas outright and the speaker has to rewrite it.
#include <threadsafe/threadsafe.h>
#include <cstdio>

struct counting_task {
    int& total;
    void operator()() const {
        for (int step = 0; step < 100'000; ++step)
            ++total;
    }
};

int main() {
    int total = 0;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task(counting_task{total});
    }
    std::printf("total = %d (expected 400000)\n", total);
}
