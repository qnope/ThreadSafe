#pragma once

#include <concepts>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
class synchronized_value;

template <class T, class Lock>
class value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    T& operator*() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");
    T* operator->() && noexcept = delete("a temporary guard is destroyed at the semicolon, so it cannot hand out a reference");

    T& operator*() const& noexcept { return *value_; }
    T* operator->() const& noexcept { return value_; }

private:
    template <class>
    friend class synchronized_value;

    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
public:
    static constexpr bool shared_readable = bool(is_synchronizable_v<const T>);

    using mutex = std::conditional_t<shared_readable, std::shared_mutex,
                                     std::mutex>;
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<
        const T, std::conditional_t<shared_readable, std::shared_lock<mutex>,
                                    std::unique_lock<mutex>>>;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {
        static_assert(sendable<T>,
                      "the mutex serializes access, but the T still crosses "
                      "thread boundaries — one thread at a time — so T must "
                      "be sendable");
    }

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static std::shared_ptr<synchronized_value>
    make(Args&&... args) {
        return std::make_shared<synchronized_value>(
            std::forward<Args>(args)...);
    }

    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable mutex mutex_;
    T value_;
};

template <class T>
struct is_unsafe_synchronizable<synchronized_value<T>>
    : std::bool_constant<is_sendable_v<T>> {};

template <class T>
struct is_unsafe_lifetime_aware<synchronized_value<T>>
    : std::bool_constant<is_lifetime_aware_v<T>> {};

}
