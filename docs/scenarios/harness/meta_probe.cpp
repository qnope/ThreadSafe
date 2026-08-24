#include <meta>
#include <cstdio>
struct S { static inline long long total = 0; static constexpr int limit = 8;
           static thread_local long long tls; int weight; };
consteval int count_static() {
    using namespace std::meta;
    int n = 0;
    for (info m : members_of(^^S, access_context::unchecked()))
        if (is_variable(m) && has_static_storage_duration(m)) ++n;
    return n;
}
static_assert(count_static() == 2, "total and limit are static-storage variables");
int main() { std::printf("ok\n"); }
