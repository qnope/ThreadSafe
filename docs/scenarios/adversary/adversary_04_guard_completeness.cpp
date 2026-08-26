#include <threadsafe/threadsafe.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// A base the author genuinely made thread-safe: every member is atomic.
// Answering the library's question the documented way -- by specializing the
// trait -- is what CLAUDE.md tells the user to do.
struct Base {
    virtual ~Base() = default;
    virtual void bump() { count_.fetch_add(1, std::memory_order_relaxed); }
    std::atomic<int> count_{0};
};

template <>
struct threadsafe::is_synchronizable<Base> : std::true_type {};

// A derived class written later, by someone else, that is NOT thread-safe.
struct Derived : Base {
    void bump() override { Base::bump(); ++unsynchronized_; }
    long unsynchronized_ = 0;
};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// unique_ptr: the guard fires -- correctly refused.
static_assert(!is_sendable_v<std::unique_ptr<Base>>);
static_assert(!is_synchronizable_v<const std::unique_ptr<Base>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::shared_ptr<Base>>);

// Every other indirection to the very same Base: the guard is absent.
static_assert(is_sendable_v<std::shared_ptr<Base>>, "shared_ptr HOLE");
static_assert(is_sendable_v<std::weak_ptr<Base>>, "weak_ptr HOLE");
static_assert(is_sendable_v<Base*>, "raw pointer HOLE");
static_assert(is_sendable_v<Base&>, "reference HOLE");
static_assert(is_sendable_v<std::reference_wrapper<Base>>, "ref_wrapper HOLE");
static_assert(is_sendable_v<std::vector<Base*>>, "vector<Base*> HOLE");
static_assert(is_synchronizable_v<const std::shared_ptr<Base>>, "const shared_ptr HOLE");
static_assert(is_synchronizable_v<const std::reference_wrapper<Base>>, "const ref_wrapper HOLE");

// ... and therefore synchronized_value hands two readers a non-const Base*
static_assert(std::is_same_v<
    threadsafe::synchronized_value<std::shared_ptr<Base>>::mutex,
    std::shared_mutex>, "shared_mutex chosen");
