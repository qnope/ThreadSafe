#include <cstdio>
#include <thread>
#include <vector>

struct Payload {
    int a;
    double b;
    long c;
};

void work(Payload payload, int extra) {
    std::printf("%d %f %ld %d\n", payload.a, payload.b, payload.c, extra);
}

void run_with_launcher(Payload payload, int extra) {
    std::vector<std::jthread> threads;
    threads.emplace_back(work, payload, extra);
}
