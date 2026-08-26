#include <threadsafe/threadsafe.h>
#include <concepts>
#include <mutex>
#include <shared_mutex>

struct ImmutablePoint { int x; int y; };
THREADSAFE_SHARE_CONST_READS(ImmutablePoint);

struct PlainPoint { int x; int y; };

static_assert(std::same_as<threadsafe::synchronized_value<ImmutablePoint>::mutex,
                           std::shared_mutex>,
              "the opt-in restores shared reads");
static_assert(std::same_as<threadsafe::synchronized_value<PlainPoint>::mutex,
                           std::mutex>,
              "without it, exclusive");
