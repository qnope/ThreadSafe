#include <threadsafe/threadsafe.h>
#include <memory>
namespace {
struct PolyBase { virtual ~PolyBase() = default; };
struct PolyFinal final : PolyBase {};
struct BadDeleter { BadDeleter(const BadDeleter&); void operator()(const int*) const; };
struct Plain { int a; };
}
using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const std::unique_ptr<const PolyBase>>, "A: base rejected");
static_assert(is_synchronizable_v<const std::unique_ptr<const PolyFinal>>, "B: final kept");
static_assert(is_synchronizable_v<const std::unique_ptr<const int>>, "C");
static_assert(is_synchronizable_v<const std::unique_ptr<const int[]>>, "D");
static_assert(is_synchronizable_v<const std::unique_ptr<const Plain>>, "E");
static_assert(!is_synchronizable_v<const std::unique_ptr<int>>, "F");
static_assert(!is_synchronizable_v<const std::unique_ptr<const int, BadDeleter>>, "G");
int main() {}
