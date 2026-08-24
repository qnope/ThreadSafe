#include <threadsafe/threadsafe.h>

#include <memory>

struct NeverDefined;

static_assert(!threadsafe::is_lifetime_aware_v<NeverDefined>,
              "an incomplete class cannot be walked");
static_assert(!threadsafe::is_lifetime_aware_v<NeverDefined[]>,
              "nor an unbounded array of one");
static_assert(!threadsafe::is_lifetime_aware_v<std::unique_ptr<NeverDefined>>,
              "nor a unique_ptr to one");
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<NeverDefined>>,
              "shared_ptr answers true even for an incomplete pointee");

int main() { return 0; }
