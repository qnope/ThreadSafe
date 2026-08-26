#include <threadsafe/threadsafe.h>
#include <atomic>

union SafeUnion      { int i_; float f_; };
union MixedUnion     { int i_; int *p_; };
union MutableUnion   { mutable int i_; };
union AtomicUnion    { int i_; std::atomic<int> a_; AtomicUnion() : i_(0) {} };
struct HoldsSafeUnion  { SafeUnion u_; };
struct HoldsMixedUnion { MixedUnion u_; };

// virtual / repeated bases
struct Root      { int value_; };
struct LeftMid   : virtual Root {};
struct RightMid  : virtual Root {};
struct Diamond   : LeftMid, RightMid {};
struct BadRoot   { int *borrowed_; };
struct LeftBad   : virtual BadRoot {};
struct RightBad  : virtual BadRoot {};
struct BadDiamond: LeftBad, RightBad {};
struct RepeatedLeft  : Root {};
struct RepeatedRight : Root {};
struct RepeatedBoth  : RepeatedLeft, RepeatedRight {};

// polymorphic
struct Poly { virtual ~Poly() = default; int value_; };
struct PolyFinal final : Poly {};
struct PolyMutable { virtual ~PolyMutable() = default; mutable int hits_; };

using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const SafeUnion>,     "SafeUnion TRUE");
static_assert(!is_synchronizable_v<const MixedUnion>,    "MixedUnion TRUE");
static_assert(!is_synchronizable_v<const MutableUnion>,  "MutableUnion TRUE");
static_assert(!is_synchronizable_v<const AtomicUnion>,   "AtomicUnion TRUE");
static_assert(!is_synchronizable_v<const HoldsSafeUnion>,"HoldsSafeUnion TRUE");
static_assert(!is_synchronizable_v<const HoldsMixedUnion>,"HoldsMixedUnion TRUE");
static_assert(!is_synchronizable_v<const Diamond>,       "Diamond TRUE");
static_assert(!is_synchronizable_v<const BadDiamond>,    "BadDiamond TRUE");
static_assert(!is_synchronizable_v<const RepeatedBoth>,  "RepeatedBoth TRUE");
static_assert(!is_synchronizable_v<const Poly>,          "Poly TRUE");
static_assert(!is_synchronizable_v<const PolyFinal>,     "PolyFinal TRUE");
static_assert(!is_synchronizable_v<const PolyMutable>,   "PolyMutable TRUE");
