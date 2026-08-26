#include <threadsafe/threadsafe.h>
#include <meta>
using namespace std::meta;
static_assert(is_array_type(^^int[4]), "int[4] not array");
static_assert(!is_array_type(^^int[0]), "int[0] IS array");
static_assert(!is_class_type(^^int[0]), "int[0] IS class");
static_assert(!is_scalar_type(^^int[0]), "int[0] IS scalar");
static_assert(!is_bounded_array_type(^^int[0]), "int[0] IS bounded array");
static_assert(!is_unbounded_array_type(^^int[0]), "int[0] IS unbounded array");
static_assert(!threadsafe::is_synchronizable_v<const int[0]>, "const int[0] TRUE");
static_assert(!threadsafe::is_sendable_v<int[0]>, "int[0] sendable TRUE");
struct ZeroSizedArray { int value_; int tail_[0]; };
static_assert(!threadsafe::is_sendable_v<ZeroSizedArray>, "ZeroSizedArray sendable TRUE");
static_assert(!threadsafe::is_lifetime_aware_v<ZeroSizedArray>, "ZeroSizedArray lifetime TRUE");
