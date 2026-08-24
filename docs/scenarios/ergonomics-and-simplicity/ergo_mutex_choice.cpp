#include <threadsafe/threadsafe.h>
#include <deque>
#include <print>
#include <shared_mutex>
#include <string>
int main() {
    std::println("synchronized_value<int>::mutex is shared_mutex   = {}",
                 std::is_same_v<threadsafe::synchronized_value<int>::mutex,
                                std::shared_mutex>);
    std::println("synchronized_value<std::deque<int>>::mutex shared = {}",
                 std::is_same_v<threadsafe::synchronized_value<std::deque<int>>::mutex,
                                std::shared_mutex>);
    std::println("sizeof(std::mutex)={} sizeof(std::shared_mutex)={}",
                 sizeof(std::mutex), sizeof(std::shared_mutex));
}
