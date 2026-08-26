// std::views::iota owns nothing and borrows nothing: it holds two integers.
// The blanket borrowed_range rule rejects it anyway.
#include <threadsafe/threadsafe.h>

#include <ranges>

using Indices = decltype(std::views::iota(0, 10));

static_assert(sizeof(Indices) == 2 * sizeof(int),
              "an iota_view is just its two bounds");
static_assert(!threadsafe::is_lifetime_aware_v<Indices>,
              "OBSERVED: the library says an index range does not own itself");
static_assert(!threadsafe::launchable_task<void (*)(Indices), Indices>,
              "OBSERVED: so it cannot be handed to a task");
static_assert(threadsafe::is_sendable_v<Indices>,
              "yet it is perfectly sendable");

consteval void explain() { threadsafe::assert_lifetime_aware<Indices>(); }
static_assert((explain(), true));

int main() {}
