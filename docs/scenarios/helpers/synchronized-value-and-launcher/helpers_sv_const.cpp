#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <type_traits>
int main() {
    threadsafe::synchronized_value<const int> frozen{5};
    auto locked = frozen.lock();
    static_assert(std::is_same_v<decltype(*locked), const int&>);
    std::printf("%d\n", *locked);
}
