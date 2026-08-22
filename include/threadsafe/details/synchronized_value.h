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

    T& operator*() const noexcept { return *value_; }
    T* operator->() const noexcept { return value_; }

private:
    template <class>
    friend class synchronized_value;

    value_guard(std::shared_mutex& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

public:
    using guard = value_guard<T, std::unique_lock<std::shared_mutex>>;
    // A T that writes under const (a mutable cache the trait cannot clear)
    // must not let two readers in at once: its lock_shared() degrades to the
    // exclusive lock.
    using const_guard =
        value_guard<const T,
                    std::conditional_t<is_synchronizable<const T>,
                                       std::shared_lock<std::shared_mutex>,
                                       std::unique_lock<std::shared_mutex>>>;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] static std::shared_ptr<synchronized_value>
    make(Args&&... args) {
        return std::make_shared<synchronized_value>(
            std::forward<Args>(args)...);
    }

    // nodiscard is load-bearing: a discarded guard is a temporary destroyed at
    // the semicolon, i.e. a lock taken and immediately released.
    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable std::shared_mutex mutex_;
    T value_;
};

template <class T>
constexpr bool is_synchronizable<synchronized_value<T>> = is_sendable<T>;

template <class T>
constexpr bool is_lifetime_aware<synchronized_value<T>> = is_lifetime_aware<T>;

template <class T, class Lock>
constexpr bool is_sendable<value_guard<T, Lock>> = false;
template <class T, class Lock>
constexpr bool is_lifetime_aware<value_guard<T, Lock>> = false;

}
