#include <thread>
#include <cstdio>
int shared = 0;
int main() {
    std::thread a([]{ for (int i = 0; i < 1000; ++i) ++shared; });
    std::thread b([]{ for (int i = 0; i < 1000; ++i) ++shared; });
    a.join(); b.join();
    printf("%d\n", shared);
}
