#include <threadsafe/threadsafe.h>

namespace mylib {

struct handle {
    void *raw;
};

// A user naturally writes the opt-in next to the type, inside their namespace.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(handle);

}

int main() {}
