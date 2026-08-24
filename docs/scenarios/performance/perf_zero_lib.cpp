#include <threadsafe/threadsafe.h>
#include <cstdio>

struct Payload {
    int a;
    double b;
    long c;
};

void work(Payload payload, int extra) {
    std::printf("%d %f %ld %d\n", payload.a, payload.b, payload.c, extra);
}

void run_with_launcher(Payload payload, int extra) {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(work, payload, extra);
}
