#include <threadsafe/threadsafe.h>

#include <memory>
#include <vector>

namespace {

// A handle that is registered with, and must be closed on, the thread that
// created it. It is perfectly safe to READ from any thread, so the structural
// const walk says yes, but it must never be destroyed elsewhere.
struct RenderHandle {
    int descriptor;
};

}

// The documented way to say "this must not cross a thread boundary".
template <>
struct threadsafe::is_sendable<RenderHandle> : std::false_type {};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(!is_sendable_v<RenderHandle>);
static_assert(!is_sendable_v<std::vector<RenderHandle>>,
              "the opt-out propagates through the container -- as intended");

// Same object, one extra top-level const, opposite answer.
static_assert(is_sendable_v<const std::vector<RenderHandle>>,
              "BUG: const laundered the opt-out away");
static_assert(threadsafe::sendable<const std::vector<RenderHandle>>);

// The laundering is reachable through the library's own vocabulary.
using Snapshot = threadsafe::synchronized_value<const std::vector<RenderHandle>>;
// (the static_assert(sendable<T>) inside synchronized_value passes)

static_assert(is_synchronizable_v<Snapshot>,
              "synchronized_value<T> is synchronizable when T is sendable");
static_assert(is_sendable_v<std::shared_ptr<Snapshot>>,
              "so the shared_ptr crosses -- and the last owner, on the other "
              "thread, runs ~RenderHandle there");
static_assert(threadsafe::launchable_task<
                  decltype([](std::shared_ptr<Snapshot>) {}),
                  std::shared_ptr<Snapshot>>,
              "launch_task accepts it");

// Whereas the un-const spelling is correctly refused everywhere:
static_assert(!threadsafe::launchable_task<
                  decltype([](std::vector<RenderHandle>) {}),
                  std::vector<RenderHandle>>);
