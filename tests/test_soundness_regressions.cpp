// Regression tests for the soundness holes found in the 2026-08-09 audit.
// Each block names the hole it pins shut; see docs/thread-safety-audit.md.
#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <complex>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

struct SyncType {};

// Borrowing shapes: each owns nothing, whatever it is wrapped in.
struct HoldsPointer {
    SyncType* p;
};
struct HoldsReference {
    SyncType& r;
};
struct HoldsRefWrapper {
    std::reference_wrapper<SyncType> r;
};
struct HoldsView {
    std::string_view sv;
};
struct DerivesFromBorrower : HoldsPointer {};

struct Owns {
    int v;
    std::string s;
};

// Empty, and therefore a safe_callable — but not sendable.
struct EmptyUserCopy {
    EmptyUserCopy() = default;
    EmptyUserCopy(const EmptyUserCopy&) {}
    void operator()() const {}
};

struct PolyBase {
    virtual ~PolyBase() = default;
};
struct PolyFinal final : PolyBase {};

struct WithCArray {
    char data[64];
    unsigned len;
};

void free_function() {}

template <class F, class... Args>
constexpr bool can_launch_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_task(f, args...);
    };

}  // namespace

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

using threadsafe::is_lifetime_aware;
using threadsafe::is_safe_callable;
using threadsafe::is_sendable;

// --- hole 1: is_lifetime_aware must be transitive -----------------------------
// One layer of composition used to launder any borrow into an "owner".
static_assert(!is_lifetime_aware<HoldsPointer>,
              "a struct holding a raw pointer owns nothing");
static_assert(!is_lifetime_aware<HoldsReference>,
              "a struct holding a reference owns nothing");
static_assert(!is_lifetime_aware<HoldsRefWrapper>,
              "a struct holding a reference_wrapper owns nothing");
static_assert(!is_lifetime_aware<HoldsView>,
              "a struct holding a view owns nothing");
static_assert(!is_lifetime_aware<DerivesFromBorrower>,
              "recursion covers base classes, not just members");
static_assert(!is_lifetime_aware<std::vector<SyncType*>>,
              "a container owns its elements, not what they point at");
static_assert(!is_lifetime_aware<std::pair<SyncType*, int>>,
              "std::pair propagates borrowing");
static_assert(!is_lifetime_aware<std::tuple<int, SyncType*>>,
              "std::tuple propagates borrowing");
static_assert(!is_lifetime_aware<std::array<SyncType*, 4>>,
              "an array of borrows is a borrow");
static_assert(!is_lifetime_aware<std::optional<std::string_view>>,
              "std::optional propagates borrowing");
static_assert(!is_lifetime_aware<std::pmr::vector<int>>,
              "a pmr container borrows its memory_resource");

// ...without turning owners into borrowers.
static_assert(is_lifetime_aware<Owns>);
static_assert(is_lifetime_aware<std::vector<std::string>>);
static_assert(is_lifetime_aware<std::deque<int>>);
static_assert(is_lifetime_aware<std::list<int>>);
static_assert(is_lifetime_aware<std::pair<int, std::string>>);
static_assert(is_lifetime_aware<std::tuple<int, std::string>>);
static_assert(is_lifetime_aware<std::array<int, 4>>);
static_assert(is_lifetime_aware<std::optional<std::string>>);
static_assert(is_lifetime_aware<std::unique_ptr<int>>);

// The use-after-free this closes: sendable, but not an owner, so launch_task
// (whose task may outlive the calling scope) must refuse it.
static_assert(is_sendable<HoldsPointer>);
static_assert(!can_launch_task<decltype([](HoldsPointer) {}), HoldsPointer>,
              "launch_task must reject a struct-wrapped borrow");
static_assert(!can_launch_task<decltype([](std::vector<SyncType*>) {}),
                               std::vector<SyncType*>>,
              "launch_task must reject a container of borrows");

// --- hole 2: type erasure through a polymorphic base ---------------------------
static_assert(!is_sendable<std::unique_ptr<PolyBase>>,
              "the dynamic type behind a non-final polymorphic base is unknown");
static_assert(is_sendable<std::unique_ptr<PolyFinal>>,
              "a final type has no unknown dynamic type");
static_assert(is_sendable<std::unique_ptr<int>>,
              "non-polymorphic pointees are unaffected");

// --- hole 3: C arrays used to be a hard error, not an answer -------------------
static_assert(is_sendable<int[4]>, "arrays follow their element type");
static_assert(!is_sendable<int*[4]>,
              "an array of non-sendable elements follows its element type too");
static_assert(is_sendable<WithCArray>, "a fixed buffer does not block sending");
static_assert(is_sendable<std::array<int, 4>>);
static_assert(is_sendable<std::mutex>,
              "a mutex may be moved to another thread");

// --- hole 4: the callable must be sendable, not merely safe to share -----------
static_assert(is_safe_callable<EmptyUserCopy>, "empty, so safe to share");
static_assert(!is_sendable<EmptyUserCopy>, "but a user-provided copy blocks it");
static_assert(!can_launch_task<EmptyUserCopy>,
              "launch_task copies the callable onto the thread and destroys it "
              "there, so F must be sendable");

// --- hole 5: std::jthread injects a stop_token behind the constraints ----------
static_assert(is_sendable<std::stop_token> && is_lifetime_aware<std::stop_token>,
              "the injected argument must satisfy the traits on its own");
static_assert(can_launch_task<decltype([](std::stop_token) {})>);

// --- function pointers: code cannot dangle ------------------------------------
static_assert(is_lifetime_aware<void (*)()>,
              "functions have static storage duration");
static_assert(can_launch_task<decltype(&free_function)>,
              "a plain function must be launchable");

// --- cv forwarding is consistent ----------------------------------------------
static_assert(is_safe_callable<void (*const)()>,
              "a cv-qualified function pointer is still a safe callable");
static_assert(!is_lifetime_aware<int* const>);
static_assert(is_lifetime_aware<const std::string>);

// --- the vocabulary types answer at all ---------------------------------------
static_assert(is_sendable<std::pair<int, std::string>>);
static_assert(is_sendable<std::tuple<int, double>>);
static_assert(is_sendable<std::optional<int>>);
static_assert(is_sendable<std::deque<int>>);
static_assert(is_sendable<std::list<int>>);
static_assert(is_sendable<std::complex<double>>,
              "__complex__ T is neither scalar nor class; it needs its own rule");
static_assert(is_sendable<std::pair<int, SyncType*>>,
              "a pointer to a synchronizable type is sendable, so the pair is");

// --- the launcher is not shareable --------------------------------------------
static_assert(!threadsafe::is_synchronizable<threadsafe::asynchronous_task_launcher>,
              "threads_ is a plain vector; launching from two threads races");
