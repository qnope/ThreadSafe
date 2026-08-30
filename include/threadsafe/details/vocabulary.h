#pragma once

#include <memory>
#include <stop_token>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_unsafe_sendable<std::allocator<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T>
struct is_unsafe_synchronizable<const std::allocator<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T>
struct is_unsafe_lifetime_aware<std::allocator<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_sendable<std::stop_token> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_sendable<std::stop_source> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_synchronizable<const std::stop_token> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_synchronizable<const std::stop_source> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_lifetime_aware<std::stop_token> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <>
struct is_unsafe_lifetime_aware<std::stop_source> {
    static consteval TraitAnswer diagnose() { return {}; }
};

}
