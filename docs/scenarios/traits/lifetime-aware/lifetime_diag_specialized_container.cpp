#include <threadsafe/threadsafe.h>

#include <vector>

consteval bool explain() {
    threadsafe::assert_lifetime_aware<std::vector<int *>>();
    return true;
}

static_assert(explain());

int main() { return 0; }
