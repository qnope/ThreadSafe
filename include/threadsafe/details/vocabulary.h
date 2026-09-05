#pragma once

#include <memory>
#include <stop_token>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_unsafe_sendable<std::allocator<T>> : std::true_type {};

template <class T>
struct is_unsafe_synchronizable<const std::allocator<T>> : std::true_type {};

template <class T>
struct is_unsafe_lifetime_aware<std::allocator<T>> : std::true_type {};

template <>
struct is_unsafe_sendable<std::stop_token> : std::true_type {};

template <>
struct is_unsafe_sendable<std::stop_source> : std::true_type {};

template <>
struct is_unsafe_synchronizable<const std::stop_token> : std::true_type {};

template <>
struct is_unsafe_synchronizable<const std::stop_source> : std::true_type {};

template <>
struct is_unsafe_lifetime_aware<std::stop_token> : std::true_type {};

template <>
struct is_unsafe_lifetime_aware<std::stop_source> : std::true_type {};

}
