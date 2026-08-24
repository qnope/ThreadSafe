// Does GCC 16 expose closure captures to reflection at all?
#include <meta>
#include <cstdio>

template <class ClosureType>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(
               ^^ClosureType, std::meta::access_context::unchecked())
        .size();
}

template <class ClosureType>
consteval bool is_empty() {
    return std::meta::is_empty_type(^^ClosureType);
}

int main() {
    int captured_value = 0;
    auto capturing_lambda = [&captured_value] { return captured_value; };
    constexpr std::size_t count = member_count<decltype(capturing_lambda)>();
    constexpr bool empty = is_empty<decltype(capturing_lambda)>();
    std::printf("members=%zu empty=%d size=%zu\n", count, (int) empty,
                sizeof(capturing_lambda));
}
