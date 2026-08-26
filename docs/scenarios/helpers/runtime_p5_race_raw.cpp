// Program 5, the racing body with the library removed, so Apple clang's
// ThreadSanitizer can see it. This is byte-for-byte the operator() of
// p5_wrong_struct.cpp:
//     struct counting_task { int& total;
//         void operator()() const { for (int step = 0; step < 5'000; ++step) ++total; } };
// and the launch loop of its main(), with asynchronous_task_launcher replaced by
// the std::vector<std::jthread> it holds internally.
// build: clang++ -std=c++20 -fsanitize=thread -g -O1 -pthread p5_race_raw.cpp -o p5_race
#include <cstdio>
#include <thread>
#include <vector>

struct counting_task {
    int& total;
    void operator()() const {
        for (int step = 0; step < 5'000; ++step)
            ++total;
    }
};

int main() {
    int total = 0;
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < 4; ++worker)
            threads.emplace_back(counting_task{total});
    }
    std::printf("total = %d (expected 20000)\n", total);
}
