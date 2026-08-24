#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <vector>

template <class T>
void describe(const char* name) {
    using sv = threadsafe::synchronized_value<T>;
    std::printf("%-28s mutex=%-18s sizeof(mutex)=%2zu  sizeof(sv)=%3zu  "
                "sizeof(guard)=%2zu sizeof(const_guard)=%2zu\n",
                name,
                std::is_same_v<typename sv::mutex, std::shared_mutex>
                    ? "std::shared_mutex"
                    : "std::mutex",
                sizeof(typename sv::mutex), sizeof(sv),
                sizeof(typename sv::guard), sizeof(typename sv::const_guard));
}

int main() {
    describe<int>("int");
    describe<double>("double");
    describe<std::string>("std::string");
    describe<std::vector<int>>("std::vector<int>");
    std::printf("\nreference sizes: std::mutex=%zu shared_mutex=%zu "
                "lock_guard<mutex>=%zu unique_lock<mutex>=%zu "
                "shared_lock<shared_mutex>=%zu\n",
                sizeof(std::mutex), sizeof(std::shared_mutex),
                sizeof(std::lock_guard<std::mutex>),
                sizeof(std::unique_lock<std::mutex>),
                sizeof(std::shared_lock<std::shared_mutex>));
}
