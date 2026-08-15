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

}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

using threadsafe::is_lifetime_aware;
using threadsafe::is_safe_callable;
using threadsafe::is_sendable;

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

static_assert(is_lifetime_aware<Owns>);
static_assert(is_lifetime_aware<std::vector<std::string>>);
static_assert(is_lifetime_aware<std::deque<int>>);
static_assert(is_lifetime_aware<std::list<int>>);
static_assert(is_lifetime_aware<std::pair<int, std::string>>);
static_assert(is_lifetime_aware<std::tuple<int, std::string>>);
static_assert(is_lifetime_aware<std::array<int, 4>>);
static_assert(is_lifetime_aware<std::optional<std::string>>);
static_assert(is_lifetime_aware<std::unique_ptr<int>>);

static_assert(is_sendable<HoldsPointer>);
static_assert(!can_launch_task<decltype([](HoldsPointer) {}), HoldsPointer>,
              "launch_task must reject a struct-wrapped borrow");
static_assert(!can_launch_task<decltype([](std::vector<SyncType*>) {}),
                               std::vector<SyncType*>>,
              "launch_task must reject a container of borrows");

static_assert(!is_sendable<std::unique_ptr<PolyBase>>,
              "the dynamic type behind a non-final polymorphic base is unknown");
static_assert(is_sendable<std::unique_ptr<PolyFinal>>,
              "a final type has no unknown dynamic type");
static_assert(is_sendable<std::unique_ptr<int>>,
              "non-polymorphic pointees are unaffected");

static_assert(is_sendable<int[4]>, "arrays follow their element type");
static_assert(!is_sendable<int*[4]>,
              "an array of non-sendable elements follows its element type too");
static_assert(is_sendable<WithCArray>, "a fixed buffer does not block sending");
static_assert(is_sendable<std::array<int, 4>>);
static_assert(is_sendable<std::mutex>,
              "a mutex may be moved to another thread");

static_assert(is_safe_callable<EmptyUserCopy>, "empty, so safe to share");
static_assert(!is_sendable<EmptyUserCopy>, "but a user-provided copy blocks it");
static_assert(!can_launch_task<EmptyUserCopy>,
              "launch_task copies the callable onto the thread and destroys it "
              "there, so F must be sendable");

static_assert(is_sendable<std::stop_token> && is_lifetime_aware<std::stop_token>,
              "the injected argument must satisfy the traits on its own");
static_assert(can_launch_task<decltype([](std::stop_token) {})>);

static_assert(is_lifetime_aware<void (*)()>,
              "functions have static storage duration");
static_assert(can_launch_task<decltype(&free_function)>,
              "a plain function must be launchable");

static_assert(is_safe_callable<void (*const)()>,
              "a cv-qualified function pointer is still a safe callable");
static_assert(!is_lifetime_aware<int* const>);
static_assert(is_lifetime_aware<const std::string>);

static_assert(is_sendable<std::pair<int, std::string>>);
static_assert(is_sendable<std::tuple<int, double>>);
static_assert(is_sendable<std::optional<int>>);
static_assert(is_sendable<std::deque<int>>);
static_assert(is_sendable<std::list<int>>);
static_assert(is_sendable<std::complex<double>>,
              "__complex__ T is neither scalar nor class; it needs its own rule");
static_assert(is_sendable<std::pair<int, SyncType*>>,
              "a pointer to a synchronizable type is sendable, so the pair is");

static_assert(!threadsafe::is_synchronizable<threadsafe::asynchronous_task_launcher>,
              "threads_ is a plain vector; launching from two threads races");
