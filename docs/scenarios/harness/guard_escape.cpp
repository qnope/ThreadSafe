// Does the SAME escape shape already exist in synchronized_value, which the
// library ships and documents? A named guard hands out T& via operator*() const&.
#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <string>

int main() {
    threadsafe::synchronized_value<std::string> value{"held"};
    std::string* escaped = nullptr;
    {
        auto guard = value.lock();
        escaped = &*guard;          // accepted: lvalue guard, not the deleted && overload
    }                                // lock released here, reference still live
    std::printf("escaped past the lock: %s\n", escaped->c_str());
    return 0;
}
