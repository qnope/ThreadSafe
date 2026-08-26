#include <meta>
#include <cstdio>
int main() {
    int a = 1, b = 2;
    auto closure = [a, &b] { return a + b; };
    using Closure = decltype(closure);
    constexpr auto context = std::meta::access_context::unchecked();
    static_assert(std::meta::nonstatic_data_members_of(^^Closure, context).size() == 0,
                  "members are visible after all");
    static_assert(!std::meta::is_empty_type(^^Closure));
    std::printf("%zu\n", std::meta::nonstatic_data_members_of(^^Closure, context).size());
}
