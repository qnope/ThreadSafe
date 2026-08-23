#pragma once

#include <array>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class A, class B>
struct is_sendable<std::pair<A, B>>
    : std::bool_constant<is_sendable_v<A> && is_sendable_v<B>> {};
template <class A, class B>
struct is_lifetime_aware<std::pair<A, B>>
    : std::bool_constant<is_lifetime_aware_v<A> && is_lifetime_aware_v<B>> {};

template <class... Ts>
struct is_sendable<std::tuple<Ts...>>
    : std::bool_constant<(is_sendable_v<Ts> && ...)> {};
template <class... Ts>
struct is_lifetime_aware<std::tuple<Ts...>>
    : std::bool_constant<(is_lifetime_aware_v<Ts> && ...)> {};

template <class T>
struct is_sendable<std::optional<T>> : std::bool_constant<is_sendable_v<T>> {};
template <class T>
struct is_lifetime_aware<std::optional<T>> : is_lifetime_aware<T> {};

template <class... Ts>
struct is_sendable<std::variant<Ts...>>
    : std::bool_constant<(is_sendable_v<Ts> && ...)> {};
template <class... Ts>
struct is_lifetime_aware<std::variant<Ts...>>
    : std::bool_constant<(is_lifetime_aware_v<Ts> && ...)> {};

template <class T, std::size_t N>
struct is_sendable<std::array<T, N>> : std::bool_constant<is_sendable_v<T>> {};
template <class T, std::size_t N>
struct is_lifetime_aware<std::array<T, N>> : is_lifetime_aware<T> {};

// These need explicit const rules only because their constructor templates
// block the structural default; the elements are held by value.
template <class A, class B>
struct is_synchronizable<const std::pair<A, B>>
    : std::bool_constant<is_synchronizable_v<const A>
                         && is_synchronizable_v<const B>> {};

template <class... Ts>
struct is_synchronizable<const std::tuple<Ts...>>
    : std::bool_constant<(is_synchronizable_v<const Ts> && ...)> {};

template <class T>
struct is_synchronizable<const std::optional<T>>
    : is_synchronizable<const T> {};

template <class... Ts>
struct is_synchronizable<const std::variant<Ts...>>
    : std::bool_constant<(is_synchronizable_v<const Ts> && ...)> {};

template <class T, std::size_t N>
struct is_synchronizable<const std::array<T, N>>
    : is_synchronizable<const T> {};

template <>
struct is_synchronizable<std::stop_token> : std::true_type {};
template <>
struct is_synchronizable<std::stop_source> : std::true_type {};

template <>
struct is_lifetime_aware<std::stop_token> : std::true_type {};
template <>
struct is_lifetime_aware<std::stop_source> : std::true_type {};

}
