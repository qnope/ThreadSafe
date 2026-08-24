// (b) Which spellings of "four threads read one immutable configuration map"
// does the library accept?
#include <threadsafe/threadsafe.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

using Configuration = std::map<std::string, std::string>;
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<const Configuration>,
              "the library agrees a const map is readable from many threads");

// ... and yet none of the five ways to hand that const map to a thread work.
static_assert(!is_sendable_v<const Configuration&>);
static_assert(!is_sendable_v<const Configuration*>);
static_assert(!is_sendable_v<std::shared_ptr<const Configuration>>);
static_assert(!is_sendable_v<std::reference_wrapper<const Configuration>>);
static_assert(!is_sendable_v<std::weak_ptr<const Configuration>>);

// What is left.
static_assert(is_sendable_v<Configuration> && is_lifetime_aware_v<Configuration>,
              "one full copy per thread");
static_assert(is_sendable_v<std::unique_ptr<const Configuration>>,
              "accepted, but a unique_ptr cannot be read by four threads");
static_assert(
    is_sendable_v<std::shared_ptr<threadsafe::synchronized_value<Configuration>>>,
    "a shared_mutex taken on every read of data nobody writes");
static_assert(is_sendable_v<threadsafe::copy_on_write<Configuration>>
                  && is_lifetime_aware_v<threadsafe::copy_on_write<Configuration>>,
              "copy_on_write is the only lock-free zero-copy share");
int main() {}
