#pragma once

#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
class copy_on_write {
public:
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
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

template <class T>
struct is_unsafe_sendable<copy_on_write<T>> {
    static consteval TraitAnswer diagnose() {
        if (const auto send_answer = is_sendable_v<T>; !send_answer)
            return send_answer.prepend_path(detail::pointee_step);

        return is_synchronizable_v<const T>.prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_lifetime_aware<copy_on_write<T>> {
    static consteval TraitAnswer diagnose() {
        return is_lifetime_aware_v<T>.prepend_path(detail::pointee_step);
    }
};

}
