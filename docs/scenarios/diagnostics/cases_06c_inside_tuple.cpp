#include <threadsafe/threadsafe.h>
#include <tuple>

struct Leaf { int *pointer; };
struct Holder { std::tuple<int, double, Leaf> parts; };

int main() {
    threadsafe::assert_sendable<Holder>();
}
