// value_guard's constructor is private, but `template <class> friend class
// synchronized_value;` befriends EVERY specialisation -- including one the user
// writes.  Explicitly specialising a library class template on a user type is
// legal, so the private constructor is reachable from outside the library.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <mutex>

struct MyTag;

namespace threadsafe {
template <>
class synchronized_value<MyTag> {
public:
    static value_guard<int, std::unique_lock<std::mutex>>
    forge(std::mutex& any_mutex, int& any_value) {
        return value_guard<int, std::unique_lock<std::mutex>>{any_mutex,
                                                              any_value};
    }
};
}

int main() {
    std::mutex unrelated_mutex;
    int unprotected_value = 7;
    const auto forged =
        threadsafe::synchronized_value<MyTag>::forge(unrelated_mutex,
                                                     unprotected_value);
    std::printf("forged guard over an unprotected int: %d\n", *forged);
    return 0;
}
