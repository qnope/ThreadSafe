#include <threadsafe/threadsafe.h>
#include <atomic>
namespace { struct Vouched {}; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Vouched);
using threadsafe::is_synchronizable_v;
static_assert(is_synchronizable_v<volatile std::atomic<int>>);
static_assert(is_synchronizable_v<const volatile std::atomic<int>>);
static_assert(is_synchronizable_v<volatile std::atomic<int>[4]>);
static_assert(is_synchronizable_v<const volatile std::atomic<int>[4]>);
static_assert(is_synchronizable_v<volatile Vouched>);
static_assert(!is_synchronizable_v<volatile int>);
static_assert(is_synchronizable_v<const volatile int>);
static_assert(is_synchronizable_v<volatile int[4]> == false);
static_assert(is_synchronizable_v<const volatile int[4]>);
static_assert(!is_synchronizable_v<volatile std::atomic<int*>>);
int main() {}
