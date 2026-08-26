#include <threadsafe/threadsafe.h>
#include <string>

struct OnlyStatics      { static int counter_; static void bump(); };
struct ZeroWidthBitfield{ int : 0; };
struct NarrowBitfield   { int flag_ : 3; };
struct UnnamedBitfield  { int : 3; };
struct MixedBitfield    { int flag_ : 3; int : 5; int other_ : 2; };
struct EmptyBase        {};
struct DerivesEmpty : EmptyBase {};
struct NoUniqueAddress  { [[no_unique_address]] EmptyBase pad_; int value_; };
struct ZeroSizedArray   { int value_; int tail_[0]; };
struct AnonymousUnion   { int tag_; union { int i_; float f_; }; };
struct AnonymousUnionBad{ int tag_; union { int i_; int *p_; }; };
struct PrivateBorrow    { private: int *borrowed_; };
struct PrivateSafe      { private: int value_; };

int captured_global = 0;

auto capture_less     = [] {};
auto capture_by_value = [x = 42] { return x; };
auto capture_by_ref   = [&captured_global] { return captured_global; };
auto capture_this_free= [p = &captured_global] { return *p; };
auto generic_lambda   = [](auto v) { return v; };
auto generic_captured = [x = 42](auto v) { return v + x; };

template <class T>
auto make_lambda_in_template(T seed) { return [seed] { return seed; }; }
using LambdaInTemplate = decltype(make_lambda_in_template(0));

using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const OnlyStatics>,       "OnlyStatics TRUE");
static_assert(!is_synchronizable_v<const ZeroWidthBitfield>, "ZeroWidthBitfield TRUE");
static_assert(!is_synchronizable_v<const NarrowBitfield>,    "NarrowBitfield TRUE");
static_assert(!is_synchronizable_v<const UnnamedBitfield>,   "UnnamedBitfield TRUE");
static_assert(!is_synchronizable_v<const MixedBitfield>,     "MixedBitfield TRUE");
static_assert(!is_synchronizable_v<const DerivesEmpty>,      "DerivesEmpty TRUE");
static_assert(!is_synchronizable_v<const NoUniqueAddress>,   "NoUniqueAddress TRUE");
static_assert(!is_synchronizable_v<const ZeroSizedArray>,    "ZeroSizedArray TRUE");
static_assert(!is_synchronizable_v<const AnonymousUnion>,    "AnonymousUnion TRUE");
static_assert(!is_synchronizable_v<const AnonymousUnionBad>, "AnonymousUnionBad TRUE");
static_assert(!is_synchronizable_v<const PrivateBorrow>,     "PrivateBorrow TRUE");
static_assert(!is_synchronizable_v<const PrivateSafe>,       "PrivateSafe TRUE");
static_assert(!is_synchronizable_v<const decltype(capture_less)>,      "capture_less TRUE");
static_assert(!is_synchronizable_v<const decltype(capture_by_value)>,  "capture_by_value TRUE");
static_assert(!is_synchronizable_v<const decltype(capture_by_ref)>,    "capture_by_ref TRUE");
static_assert(!is_synchronizable_v<const decltype(capture_this_free)>, "capture_this_free TRUE");
static_assert(!is_synchronizable_v<const decltype(generic_lambda)>,    "generic_lambda TRUE");
static_assert(!is_synchronizable_v<const decltype(generic_captured)>,  "generic_captured TRUE");
static_assert(!is_synchronizable_v<const LambdaInTemplate>,            "LambdaInTemplate TRUE");
