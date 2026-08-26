#include <threadsafe/threadsafe.h>
#include <meta>
#include <vector>

namespace {
struct OptedOut {};
}
template <> struct threadsafe::is_sendable<std::vector<OptedOut>> : std::false_type {};

// does the constrained partial specialization key match a cv-qualified spelling?
static_assert(threadsafe::detail::std_wrapper<std::vector<OptedOut>>);
static_assert(threadsafe::detail::std_wrapper<const std::vector<OptedOut>>,
              "the std_wrapper concept accepts a top-level-const spelling too");
static_assert(threadsafe::detail::is_allowed_std_wrapper(^^const std::vector<int>));
static_assert(std::meta::has_template_arguments(^^const std::vector<int>));

// which makes std_wrapper_is_sendable ask the const question by mistake
static_assert(threadsafe::is_synchronizable_v<const std::vector<OptedOut>>,
              "const vector<OptedOut> is READ-safe...");
static_assert(!threadsafe::is_synchronizable_v<std::vector<OptedOut>>,
              "...but not fully synchronizable");
static_assert(threadsafe::detail::std_wrapper_is_sendable(^^const std::vector<OptedOut>),
              "yet std_wrapper_is_sendable short-circuits on the const answer");

static_assert(!threadsafe::is_sendable_v<std::vector<OptedOut>>);
static_assert(threadsafe::is_sendable_v<const std::vector<OptedOut>>,
              "THE BUG: adding const flips the answer");
static_assert(threadsafe::sendable<const std::vector<OptedOut>>);
static_assert(threadsafe::is_sendable_type(^^const std::vector<OptedOut>));
