#include <threadsafe/threadsafe.h>
#include <vector>
namespace { struct Borrower { int x; }; }
template <> struct threadsafe::is_lifetime_aware<std::vector<Borrower>> : std::false_type {};
using threadsafe::is_lifetime_aware_v;
static_assert(!is_lifetime_aware_v<std::vector<Borrower>>);
static_assert(is_lifetime_aware_v<const std::vector<Borrower>>, "same laundering on lifetime_aware");
