#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <type_traits>

namespace {
// A type the user vouched sendable, with a greedy forwarding constructor.
struct Sink {
    int tag = 0;
    Sink() = default;
    template <class U>
    explicit Sink(U&&) : tag(1) {}
};
}
template <> struct threadsafe::is_sendable<Sink> : std::true_type {};

using sync_sink = threadsafe::synchronized_value<Sink>;
using cow_sink = threadsafe::copy_on_write<Sink>;

static_assert(!std::is_copy_constructible_v<sync_sink>);
static_assert(std::is_move_constructible_v<sync_sink>,
              "the greedy variadic constructor makes the deliberately immovable "
              "wrapper answer `movable` -- and it is not a move");

int main() {
    sync_sink original{};
    sync_sink surprising{original};   // NOT a copy: builds a Sink from the wrapper
    auto locked = surprising.lock();
    std::printf("synchronized_value<Sink>{other} produced tag = %d (0 = copy, 1 = hijacked)\n",
                (*locked).tag);

    cow_sink cow_original{};
    cow_sink cow_copy{cow_original};  // copy_on_write guards this shape
    std::printf("copy_on_write<Sink>{other} produced tag = %d\n", (*cow_copy).tag);
}
