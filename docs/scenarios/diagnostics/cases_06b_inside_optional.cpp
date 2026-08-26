#include <threadsafe/threadsafe.h>
#include <optional>

struct Leaf { int *pointer; };
struct Holder { std::optional<Leaf> leaf; };

int main() {
    threadsafe::assert_sendable<Holder>();
}
