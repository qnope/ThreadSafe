#include <chrono>
#include <cstdio>
#include <thread>

static int shared_value = 0;

int main() {
    std::thread writer([] {
        for (int i = 0; i < 200000; ++i) shared_value += 1;
    });
    for (int i = 0; i < 200000; ++i) shared_value += 2;
    writer.join();
    std::printf("shared_value=%d\n", shared_value);
}
