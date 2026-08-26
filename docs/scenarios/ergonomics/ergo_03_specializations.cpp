#include <threadsafe/threadsafe.h>

#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

// ---- 1. a plain type ----
namespace acme {
struct Handle {
    int descriptor;
    ~Handle() {}
};
}
template <>
struct threadsafe::is_sendable<acme::Handle> : std::true_type {};

// ---- 2. a class TEMPLATE: does a partial specialization work? ----
namespace acme {
template <class Element>
class Ring {
public:
    ~Ring() {}
private:
    std::vector<Element> slots_;
};
}
template <class Element>
struct threadsafe::is_sendable<acme::Ring<Element>>
    : threadsafe::is_sendable<Element> {};
template <class Element>
struct threadsafe::is_lifetime_aware<acme::Ring<Element>>
    : threadsafe::is_lifetime_aware<Element> {};

// ---- 3. a type in a NESTED namespace ----
namespace acme::io::detail {
struct Channel {
    std::string endpoint;
    ~Channel() {}
};
}
template <>
struct threadsafe::is_sendable<acme::io::detail::Channel> : std::true_type {};

// ---- 4. a type with a MUTABLE member you want to vouch for ----
namespace acme {
class MemoizedSize {
public:
    std::size_t size() const {
        std::lock_guard<std::mutex> held{cache_mutex_};
        return cached_size_;
    }
private:
    mutable std::mutex cache_mutex_;
    mutable std::size_t cached_size_ = 0;
};
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(acme::MemoizedSize);

static_assert(threadsafe::is_sendable_v<acme::Handle>);
static_assert(threadsafe::is_sendable_v<acme::Ring<int>>);
static_assert(!threadsafe::is_sendable_v<acme::Ring<int*>>);
static_assert(threadsafe::is_lifetime_aware_v<acme::Ring<std::string>>);
static_assert(threadsafe::is_sendable_v<acme::io::detail::Channel>);
static_assert(threadsafe::is_synchronizable_v<acme::MemoizedSize>);
static_assert(threadsafe::is_synchronizable_v<const acme::MemoizedSize>);
static_assert(threadsafe::is_sendable_v<acme::MemoizedSize>);
