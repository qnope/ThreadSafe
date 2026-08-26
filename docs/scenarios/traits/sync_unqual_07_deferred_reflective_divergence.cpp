#include <threadsafe/threadsafe.h>

#include <cstdio>

namespace {

// A type the library cannot judge on its own: it hands out a raw pointer.
struct Registry {
    int* shared_counter;
};

// Two structurally identical holders. The only difference is *when* they are
// asked the question, relative to the vouch below.
struct EarlyHolder { Registry registry; };
struct LateHolder  { Registry registry; };

}

// Asked BEFORE the vouch. This instantiates threadsafe::is_synchronizable<Registry>
// reflectively, through detail::trait_value / std::meta::substitute -- never by
// naming it, so the compiler has no syntactic use to complain about.
constexpr bool early_answer = threadsafe::is_sendable_v<EarlyHolder>;

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Registry);

// Asked AFTER the vouch.
constexpr bool late_answer = threadsafe::is_sendable_v<LateHolder>;

// Same members, same vouch, two different answers -- silently.
static_assert(early_answer == false);
static_assert(late_answer == true);
static_assert(early_answer != late_answer);

// And the type asked early keeps the stale answer for the rest of the TU:
static_assert(!threadsafe::is_sendable_v<EarlyHolder>);
static_assert(threadsafe::is_synchronizable_v<Registry>);

int main() {
    std::printf("is_sendable_v<EarlyHolder> = %s\n", early_answer ? "true" : "false");
    std::printf("is_sendable_v<LateHolder>  = %s\n", late_answer ? "true" : "false");
}
