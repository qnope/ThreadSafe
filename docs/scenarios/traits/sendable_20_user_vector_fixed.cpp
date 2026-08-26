#include <threadsafe/threadsafe.h>

#include <cstddef>
#include <string>
#include <utility>

namespace app {

template <class T>
class small_vector {
public:
    small_vector() = default;
    small_vector(const small_vector& other) : data_(new T[other.size_]), size_(other.size_) {
        for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
    }
    small_vector(small_vector&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}
    ~small_vector() { delete[] data_; }

private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
};

}

// The complete opt-in a user must write, all three traits plus the const one.
namespace threadsafe {
template <class T>
struct is_sendable<app::small_vector<T>> : is_sendable<T> {};

template <class T>
struct is_synchronizable<const app::small_vector<T>>
    : is_synchronizable<const T> {};

template <class T>
struct is_lifetime_aware<app::small_vector<T>> : is_lifetime_aware<T> {};
}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

static_assert(is_sendable_v<app::small_vector<int>>);
static_assert(is_sendable_v<app::small_vector<std::string>>);
static_assert(!is_sendable_v<app::small_vector<int*>>, "still propagates the element");
static_assert(is_synchronizable_v<const app::small_vector<int>>);
static_assert(!is_synchronizable_v<const app::small_vector<int*>>);
static_assert(is_lifetime_aware_v<app::small_vector<std::string>>);
static_assert(!is_lifetime_aware_v<app::small_vector<int*>>);
static_assert(threadsafe::launchable_task<
                  decltype([](app::small_vector<std::string>) {}),
                  app::small_vector<std::string>>);
static_assert(!threadsafe::launchable_task<
                  decltype([](app::small_vector<int*>) {}),
                  app::small_vector<int*>>);
// and synchronized_value now picks shared_mutex for the read-safe element
static_assert(std::is_same_v<
                  threadsafe::synchronized_value<app::small_vector<int>>::mutex,
                  std::shared_mutex>);
