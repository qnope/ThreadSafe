#include <threadsafe/threadsafe.h>
#include <vector>
#include <utility>

// The coinductive answer must not launder a real culprit reachable through the cycle.
struct SafeTree   { int value_; std::vector<SafeTree> children_; };
struct MutableTree{ mutable int hits_; std::vector<MutableTree> children_; };
struct BorrowTree { int *borrowed_; std::vector<BorrowTree> children_; };
struct PairTree   { std::vector<std::pair<PairTree, int *>> children_; };
struct DeepTree   { struct Inner { std::vector<DeepTree> back_; int *borrowed_; }; Inner inner_; };

using threadsafe::is_synchronizable_v;
static_assert( is_synchronizable_v<const SafeTree>,    "SafeTree FALSE");
static_assert(!is_synchronizable_v<const MutableTree>, "MutableTree TRUE");
static_assert(!is_synchronizable_v<const BorrowTree>,  "BorrowTree TRUE");
static_assert(!is_synchronizable_v<const PairTree>,    "PairTree TRUE");
static_assert(!is_synchronizable_v<const DeepTree>,    "DeepTree TRUE");
static_assert( threadsafe::is_sendable_v<SafeTree>,    "SafeTree not sendable");
static_assert(!threadsafe::is_sendable_v<BorrowTree>,  "BorrowTree sendable");
static_assert( threadsafe::is_lifetime_aware_v<SafeTree>,   "SafeTree not lifetime aware");
static_assert(!threadsafe::is_lifetime_aware_v<BorrowTree>, "BorrowTree lifetime aware");
