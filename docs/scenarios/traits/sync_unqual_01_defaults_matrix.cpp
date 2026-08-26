#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstddef>

namespace {
enum PlainEnum { plain_enum_value };
enum class ScopedEnum { scoped_enum_value };
struct Incomplete;
union SimpleUnion {
    int as_int;
    double as_double;
};
struct Empty {};
using CaptureLessLambda = decltype([] {});
using CapturingLambda = decltype([captured = 42] { return captured; });
}

using threadsafe::is_synchronizable_v;

// Fundamental types
static_assert(!is_synchronizable_v<int>);
static_assert(!is_synchronizable_v<bool>);
static_assert(!is_synchronizable_v<char>);
static_assert(!is_synchronizable_v<double>);
static_assert(!is_synchronizable_v<std::nullptr_t>);
static_assert(!is_synchronizable_v<void>);

// Enums
static_assert(!is_synchronizable_v<PlainEnum>);
static_assert(!is_synchronizable_v<ScopedEnum>);

// Function types
static_assert(is_synchronizable_v<void()>);
static_assert(is_synchronizable_v<int(int, double)>);
static_assert(is_synchronizable_v<void() noexcept>);
static_assert(is_synchronizable_v<void(...)>);
static_assert(is_synchronizable_v<void() const>);   // abominable
static_assert(is_synchronizable_v<void() &&>);      // abominable

// References and pointers
static_assert(!is_synchronizable_v<int&>);
static_assert(!is_synchronizable_v<int&&>);
static_assert(!is_synchronizable_v<int*>);
static_assert(!is_synchronizable_v<void*>);
static_assert(!is_synchronizable_v<void (*)()>);
static_assert(!is_synchronizable_v<void (&)()>);

// Arrays
static_assert(!is_synchronizable_v<int[4]>);
static_assert(!is_synchronizable_v<int[]>);
static_assert(is_synchronizable_v<std::atomic<int>[4]>);
static_assert(is_synchronizable_v<std::atomic<int>[]>);
static_assert(!is_synchronizable_v<int[2][3]>);
static_assert(is_synchronizable_v<std::atomic<int>[2][3]>);
static_assert(is_synchronizable_v<const int[2][3]>);
static_assert(is_synchronizable_v<const int[2][3][4]>);
static_assert(is_synchronizable_v<const std::atomic<int>[2][3]>);
static_assert(!is_synchronizable_v<int* const[2][3]>);

// Incomplete / unions / class types
static_assert(!is_synchronizable_v<Incomplete>);
static_assert(!is_synchronizable_v<const Incomplete>);
static_assert(!is_synchronizable_v<SimpleUnion>);
static_assert(is_synchronizable_v<const SimpleUnion>);
static_assert(!is_synchronizable_v<Empty>);
static_assert(is_synchronizable_v<const Empty>);

// Lambdas
static_assert(!is_synchronizable_v<CaptureLessLambda>);
static_assert(is_synchronizable_v<const CaptureLessLambda>);

// Member pointers
struct Host { int field; void method(); };
static_assert(!is_synchronizable_v<int Host::*>);
static_assert(is_synchronizable_v<int Host::* const>);
static_assert(is_synchronizable_v<void (Host::* const)()>);

int main() {}
