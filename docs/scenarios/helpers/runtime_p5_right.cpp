// Program 5, THE RIGHT VERSION -- slide 2 of the talk.
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p5_right.cpp -o p5_right && ./p5_right
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>

using counter = threadsafe::synchronized_value<int>;

struct counting_task {
    std::shared_ptr<counter> total;

    void operator()() const {
        for (int step = 0; step < 100'000; ++step) {
            auto guard = total->lock();
            *guard += 1;
        }
    }
};

int main() {
    auto total = counter::make(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task(counting_task{total});
    }
    const auto guard = total->lock_shared();
    std::printf("total = %d (expected 400000) -> %s\n", *guard,
                *guard == 400'000 ? "correct" : "WRONG");
}
