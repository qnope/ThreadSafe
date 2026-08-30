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
    static_assert(sendable<T>,
                  "the mutex serializes access, but the T still crosses thread "
                  "boundaries — one thread at a time — so T must be sendable");

public:
    static consteval auto get_mutex_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^std::shared_mutex;
        } else {
            return ^^std::mutex;
        }
    }

    using mutex = [:get_mutex_type():];

    static consteval auto get_const_guard_type() {
        if constexpr (is_synchronizable_v<const T>) {
            return ^^value_guard<const T, std::shared_lock<mutex>>;
        } else {
            return ^^value_guard<const T, std::unique_lock<mutex>>;
        }
    }

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = [:get_const_guard_type():];

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

    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable mutex mutex_;
    T value_;
};

template <class T>
struct is_unsafe_synchronizable<synchronized_value<T>> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<T>.prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_lifetime_aware<synchronized_value<T>> {
    static consteval TraitAnswer diagnose() {
        return is_lifetime_aware_v<T>.prepend_path(detail::pointee_step);
    }
};

template <class T, class Lock>
struct is_unsafe_sendable<value_guard<T, Lock>> {
    static consteval TraitAnswer diagnose() {
        return "a value_guard holds a lock owned by the thread that took it";
    }
};

template <class T, class Lock>
struct is_unsafe_lifetime_aware<value_guard<T, Lock>> {
    static consteval TraitAnswer diagnose() {
        return "a value_guard points into the synchronized_value it guards "
               "instead of keeping it alive";
    }
};

}
