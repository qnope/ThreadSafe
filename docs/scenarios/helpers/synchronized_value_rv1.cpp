#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::synchronized_value<int> sv{0};
    auto& r = *sv.lock();
    (void)r;
}
