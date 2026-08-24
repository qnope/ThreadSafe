#include <threadsafe/threadsafe.h>

#include <array>
#include <bitset>
#include <print>
#include <queue>
#include <stack>
#include <string_view>
#include <valarray>
#include <vector>

template <class T>
void report(const char *label) {
    std::println("{:44} lifetime_aware={:5} sendable={:5}", label,
                 threadsafe::is_lifetime_aware_v<T>,
                 threadsafe::is_sendable_v<T>);
}

int main() {
    report<std::stack<int>>("std::stack<int>");
    report<std::queue<int>>("std::queue<int>");
    report<std::priority_queue<int>>("std::priority_queue<int>");
    report<std::stack<int *>>("std::stack<int*>");
    report<std::valarray<int>>("std::valarray<int>");
    report<std::bitset<64>>("std::bitset<64>");
    report<std::array<std::string_view, 0>>("std::array<std::string_view, 0>");
    report<std::array<std::string_view, 4>>("std::array<std::string_view, 4>");
    report<std::vector<std::string_view>>("std::vector<std::string_view>");
    return 0;
}
