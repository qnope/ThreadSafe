#include <threadsafe/threadsafe.h>
#include <cstdio>
int main() {
    threadsafe::synchronized_value<int[4]> buffer;
    auto locked = buffer.lock();
    (*locked)[0] = 7;
    std::printf("%d\n", (*locked)[0]);
}
