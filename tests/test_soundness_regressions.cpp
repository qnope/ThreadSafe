#include <threadsafe/threadsafe.h>

#include <array>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
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
struct DerivesFromEmptyUserCopy : EmptyUserCopy {};

struct PolyBase {
    virtual ~PolyBase() = default;
};
struct PolyFinal final : PolyBase {};

struct WithCArray {
    char data[64];
    unsigned len;
};

void free_function() {}

[[maybe_unused]] auto borrow(std::string& s) {
    return [&s] { s += "x"; };
}
using CapturesReference = decltype(borrow(std::declval<std::string&>()));

}

template <>
struct threadsafe::is_unsafe_synchronizable<SyncType> : std::true_type {};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::launchable_task;

static_assert(!is_lifetime_aware_v<HoldsPointer>,
              "a struct holding a raw pointer owns nothing");
static_assert(!is_lifetime_aware_v<HoldsReference>,
              "a struct holding a reference owns nothing");
static_assert(!is_lifetime_aware_v<HoldsRefWrapper>,
              "a struct holding a reference_wrapper owns nothing");
static_assert(!is_lifetime_aware_v<HoldsView>,
              "a struct holding a view owns nothing");
static_assert(!is_lifetime_aware_v<DerivesFromBorrower>,
              "recursion covers base classes, not just members");
static_assert(!is_lifetime_aware_v<std::vector<SyncType*>>,
              "a container owns its elements, not what they point at");
static_assert(!is_lifetime_aware_v<std::pair<SyncType*, int>>,
              "std::pair propagates borrowing");
static_assert(!is_lifetime_aware_v<std::tuple<int, SyncType*>>,
              "std::tuple propagates borrowing");
static_assert(!is_lifetime_aware_v<std::array<SyncType*, 4>>,
              "an array of borrows is a borrow");
static_assert(!is_lifetime_aware_v<std::optional<std::string_view>>,
              "std::optional propagates borrowing");
static_assert(!is_lifetime_aware_v<std::pmr::vector<int>>,
              "a pmr container borrows its memory_resource");

static_assert(is_lifetime_aware_v<Owns>);
static_assert(is_lifetime_aware_v<std::vector<std::string>>);
static_assert(is_lifetime_aware_v<std::deque<int>>);
static_assert(is_lifetime_aware_v<std::list<int>>);
static_assert(is_lifetime_aware_v<std::pair<int, std::string>>);
static_assert(is_lifetime_aware_v<std::tuple<int, std::string>>);
static_assert(is_lifetime_aware_v<std::array<int, 4>>);
static_assert(is_lifetime_aware_v<std::optional<std::string>>);
static_assert(is_lifetime_aware_v<std::unique_ptr<int>>);

static_assert(is_sendable_v<HoldsPointer>);
static_assert(!launchable_task<decltype([](HoldsPointer) {}), HoldsPointer>,
              "launch_task must reject a struct-wrapped borrow");
static_assert(!launchable_task<decltype([](std::vector<SyncType*>) {}),
                               std::vector<SyncType*>>,
              "launch_task must reject a container of borrows");

static_assert(!is_sendable_v<std::unique_ptr<PolyBase>>,
              "the dynamic type behind a non-final polymorphic base is unknown");
static_assert(is_sendable_v<std::unique_ptr<PolyFinal>>,
              "a final type has no unknown dynamic type");
static_assert(is_sendable_v<std::unique_ptr<int>>,
              "non-polymorphic pointees are unaffected");

static_assert(!is_synchronizable_v<const std::unique_ptr<const PolyBase>>,
              "the const question may not trust a pointee whose dynamic type is "
              "unknown either: a derived object may hold a mutable member");
static_assert(is_synchronizable_v<const std::unique_ptr<const PolyFinal>>,
              "a final pointee has no unknown dynamic type");
static_assert(is_synchronizable_v<const std::unique_ptr<const int>>,
              "non-polymorphic pointees are unaffected");

static_assert(!is_lifetime_aware_v<std::unique_ptr<PolyBase>>,
              "a derived object may borrow what the base owns");
static_assert(!is_lifetime_aware_v<std::shared_ptr<PolyBase>>,
              "shared ownership does not make the derived object an owner");
static_assert(!is_lifetime_aware_v<std::weak_ptr<PolyBase>>,
              "a weak_ptr locks into the same unknown object");
static_assert(is_lifetime_aware_v<std::unique_ptr<PolyFinal>>
                  && is_lifetime_aware_v<std::shared_ptr<PolyFinal>>,
              "a final pointee has no unknown dynamic type");

static_assert(is_sendable_v<int[4]>, "arrays follow their element type");
static_assert(!is_sendable_v<int*[4]>,
              "an array of non-sendable elements follows its element type too");
static_assert(is_sendable_v<WithCArray>, "a fixed buffer does not block sending");
static_assert(is_sendable_v<std::array<int, 4>>);
static_assert(is_sendable_v<std::mutex>,
              "a mutex may be moved to another thread");

static_assert(!is_sendable_v<EmptyUserCopy>,
              "empty is not enough: the copy launch_task makes onto the thread "
              "runs a user-provided constructor there");
static_assert(!launchable_task<EmptyUserCopy>,
              "launch_task copies the callable onto the thread and destroys it "
              "there, so F must be sendable");

static_assert(std::is_empty_v<DerivesFromEmptyUserCopy>);
static_assert(!is_sendable_v<DerivesFromEmptyUserCopy>,
              "an empty class inherits its base's user-provided copy");
static_assert(!launchable_task<DerivesFromEmptyUserCopy>);

static_assert(!is_sendable_v<CapturesReference>,
              "a closure reflects no members whatever it captures, so its "
              "state is state the traits cannot inspect");
static_assert(!launchable_task<CapturesReference>,
              "a callable borrowing a local must not outlive the launcher");

static_assert(is_sendable_v<std::stop_token> && is_lifetime_aware_v<std::stop_token>,
              "the injected argument must satisfy the traits on its own");
static_assert(launchable_task<decltype([](std::stop_token) {})>);

static_assert(is_lifetime_aware_v<void (*)()>,
              "functions have static storage duration");
static_assert(launchable_task<decltype(&free_function)>,
              "a plain function must be launchable");

static_assert(is_sendable_v<void (*const)()>,
              "a cv-qualified function pointer is still sendable");
static_assert(!is_lifetime_aware_v<int* const>);
static_assert(is_lifetime_aware_v<const std::string>);

static_assert(is_sendable_v<std::pair<int, std::string>>);
static_assert(is_sendable_v<std::tuple<int, double>>);
static_assert(is_sendable_v<std::optional<int>>);
static_assert(is_sendable_v<std::deque<int>>);
static_assert(is_sendable_v<std::list<int>>);
static_assert(is_sendable_v<std::pair<int, SyncType*>>,
              "a pointer to a synchronizable type is sendable, so the pair is");

static_assert(!threadsafe::is_synchronizable_v<threadsafe::asynchronous_task_launcher>,
              "threads_ is a plain vector; launching from two threads races");

static_assert(is_synchronizable_v<const std::pair<int, std::string>>
                  && is_synchronizable_v<const std::tuple<int, double>>
                  && is_synchronizable_v<const std::optional<int>>
                  && is_synchronizable_v<const std::variant<int, std::string>>
                  && is_synchronizable_v<const std::array<int, 4>>,
              "is_synchronizable — the vocabulary types need explicit const "
              "rules only because their constructor templates block the "
              "structural default");
static_assert(!is_synchronizable_v<const std::tuple<int, int*>>
                  && !is_synchronizable_v<const std::optional<int*>>,
              "is_synchronizable — an element that borrows gives readers a "
              "write path");
static_assert(is_synchronizable_v<const std::stop_token>,
              "is_synchronizable — a full specialization satisfies the first "
              "disjunct, no const rule needed");
