#pragma once

#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/sendable.h>
#include <threadsafe/synchronizable_base.h>
#include <threadsafe/utils.h>

namespace threadsafe {

template <class T>
class copy_on_write {
public:
    // No shared_ptr enters the type: a caller keeping its own would hold a write
    // path the detach never sees, and would pin the count above one forever.
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_.get(); }

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            // use_count() is a relaxed load; the acquire pairs with the
            // releasing decrements of the copies that are gone, so their reads
            // of *ptr_ happen before the writes this reference opens.
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

// Two handles sharing a T read it at the same time, and only read it — mutation
// always detaches. That is enough for a sendable T, unless the T writes from a
// const method, which mutable state is the visible form of.
template <class T>
constexpr bool is_sendable<copy_on_write<T>> =
    is_sendable<T>
    && (is_synchronizable<T> || !detail::has_mutable_state(^^T));

template <class T>
constexpr bool is_lifetime_aware<copy_on_write<T>> = is_lifetime_aware<T>;

}
