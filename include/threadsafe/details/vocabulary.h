#pragma once

#include <memory>
#include <stop_token>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

// std::allocator is stateless -- allowed_std_wrappers cannot say this,
// because it is true even for a T that answers no.
template <class T>
struct is_unsafe_sendable<std::allocator<T>> {
    static constexpr TraitAnswer value = {};
};

template <class T>
struct is_unsafe_synchronizable<const std::allocator<T>> {
    static constexpr TraitAnswer value = {};
};

template <class T>
struct is_unsafe_lifetime_aware<std::allocator<T>> {
    static constexpr TraitAnswer value = {};
};

template <>
struct is_unsafe_sendable<std::stop_token> {
    static constexpr TraitAnswer value = {};
};

template <>
struct is_unsafe_sendable<std::stop_source> {
    static constexpr TraitAnswer value = {};
};

template <>
struct is_unsafe_synchronizable<const std::stop_token> {
    static constexpr TraitAnswer value = {};
};

template <>
struct is_unsafe_synchronizable<const std::stop_source> {
    static constexpr TraitAnswer value = {};
};

template <>
struct is_unsafe_lifetime_aware<std::stop_token> {
    static constexpr TraitAnswer value = {};
};

template <>
struct is_unsafe_lifetime_aware<std::stop_source> {
    static constexpr TraitAnswer value = {};
};

}
