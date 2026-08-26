#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <vector>
namespace {
struct Incomplete;
struct Pimpl { std::unique_ptr<Incomplete> p; };
struct Leaf { int* borrowed; };
struct Nested { Leaf leaf; };
struct Deep { Nested n; };
struct OptOut { int v; };
}
template <> struct threadsafe::is_sendable<OptOut> : std::false_type {};
static_assert((threadsafe::assert_sendable<const OptOut>(), true));
