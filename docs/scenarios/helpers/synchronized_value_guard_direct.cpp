#include <threadsafe/threadsafe.h>
#include <mutex>
int main() {
    std::mutex m;
    int v = 0;
    threadsafe::value_guard<int, std::unique_lock<std::mutex>> forged{m, v};
    (void)forged;
}
