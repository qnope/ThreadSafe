#include <memory>
#include <type_traits>
// A const std::shared_ptr<T> hands out a non-const T&, so demanding
// is_synchronizable<T> (not the const question) is the right rule.
static_assert(std::is_same_v<decltype(*std::declval<const std::shared_ptr<int> &>()), int &>);
static_assert(std::is_same_v<decltype(std::declval<const std::shared_ptr<int> &>().get()), int *>);
int main() {}
