#include <threadsafe/threadsafe.h>
#include <queue>
consteval bool explain() {
    threadsafe::assert_sendable<std::queue<int>>();
    return true;
}
static_assert(explain());
int main() {}
