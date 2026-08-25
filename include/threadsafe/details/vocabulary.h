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
struct is_sendable<std::allocator<T>> : std::true_type {};
template <class T>
struct is_synchronizable<const std::allocator<T>> : std::true_type {};
template <class T>
struct is_lifetime_aware<std::allocator<T>> : std::true_type {};

// [stoptoken.general] promises only that request_stop, stop_requested and
// stop_possible are race-free. Both types are refcounted handles whose copy
// assignment and swap touch the shared state's reference count, so the
// unqualified trait -- which blesses writing through a shared `T&` -- must stay
// false. Sending a handle to another thread is a copy, and reading one through
// const is race-free, so those two are stated directly.
template <>
struct is_sendable<std::stop_token> : std::true_type {};
template <>
struct is_sendable<std::stop_source> : std::true_type {};

template <>
struct is_synchronizable<const std::stop_token> : std::true_type {};
template <>
struct is_synchronizable<const std::stop_source> : std::true_type {};

template <>
struct is_lifetime_aware<std::stop_token> : std::true_type {};
template <>
struct is_lifetime_aware<std::stop_source> : std::true_type {};

}
