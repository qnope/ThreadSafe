#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<const std::vector<int>>>,
              "the canonical read-only sharing idiom is rejected");
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<std::vector<int>>>,
              "and the mutable one is rejected identically -- const is stripped");
