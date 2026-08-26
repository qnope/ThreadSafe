#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <vector>
namespace {
struct PolyBase { virtual ~PolyBase() = default; };
struct Leaf { int* borrowed; };
struct OptOut { int v; };
struct HoldsUnique { std::unique_ptr<PolyBase> p; };
struct HoldsVec { std::vector<OptOut> v; };
struct HoldsShared { std::shared_ptr<int> p; };
struct HoldsRefWrap { std::reference_wrapper<int> r; };
}
template <> struct threadsafe::is_sendable<std::vector<OptOut>> : std::false_type {};
static_assert((threadsafe::assert_sendable<HoldsRefWrap>(), true));
