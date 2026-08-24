#include <threadsafe/threadsafe.h>

#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

using threadsafe::is_lifetime_aware_v;

template <class T>
void report(const char* name) {
    std::cout << (is_lifetime_aware_v<T> ? "owns    " : "borrows ") << name << '\n';
}

int main() {
    report<std::span<int>>("span<int>                 ");
    report<std::vector<std::span<int>>>("vector<span<int>>         ");
    report<std::optional<std::span<int>>>("optional<span<int>>       ");
    report<std::pair<int, std::span<int>>>("pair<int, span<int>>      ");
    report<std::tuple<std::span<int>>>("tuple<span<int>>          ");
    report<std::array<std::span<int>, 2>>("array<span<int>, 2>       ");
    report<std::span<int>[2]>("span<int>[2]              ");
    report<std::unique_ptr<std::span<int>>>("unique_ptr<span<int>>     ");
    report<std::shared_ptr<std::span<int>>>("shared_ptr<span<int>>     ");
    report<std::weak_ptr<std::span<int>>>("weak_ptr<span<int>>       ");
}
