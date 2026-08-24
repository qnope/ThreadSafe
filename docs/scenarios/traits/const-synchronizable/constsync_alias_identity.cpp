#include <meta>
#include <vector>
#include <type_traits>

template <class T>
consteval bool probe() {
    using unqualified = std::remove_cv_t<T>;
    return ^^unqualified == std::meta::remove_cv(^^T);
}
template <class T>
consteval bool probe_dealias() {
    using unqualified = std::remove_cv_t<T>;
    return std::meta::dealias(^^unqualified) == std::meta::remove_cv(^^T);
}
static_assert(!probe<const std::vector<int*>>(), "alias reflection differs");
static_assert(probe_dealias<const std::vector<int*>>(), "dealias fixes it");
int main() {}
