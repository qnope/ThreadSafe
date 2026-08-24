#include <threadsafe/threadsafe.h>

#include <vector>

// The commonest mistake there is: a container of raw pointers handed to a
// thread. assert_sendable promises to "name the subobject responsible".
consteval bool explain() {
    threadsafe::assert_sendable<std::vector<int*>>();
    return true;
}

static_assert(explain());
