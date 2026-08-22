#pragma once

#include <array>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

template <class A, class B>
constexpr bool is_sendable<std::pair<A, B>> = is_sendable<A> && is_sendable<B>;
template <class A, class B>
constexpr bool is_lifetime_aware<std::pair<A, B>> =
    is_lifetime_aware<A> && is_lifetime_aware<B>;

template <class... Ts>
constexpr bool is_sendable<std::tuple<Ts...>> = (is_sendable<Ts> && ...);
template <class... Ts>
constexpr bool is_lifetime_aware<std::tuple<Ts...>> =
    (is_lifetime_aware<Ts> && ...);

template <class T>
constexpr bool is_sendable<std::optional<T>> = is_sendable<T>;
template <class T>
constexpr bool is_lifetime_aware<std::optional<T>> = is_lifetime_aware<T>;

template <class... Ts>
constexpr bool is_sendable<std::variant<Ts...>> = (is_sendable<Ts> && ...);
template <class... Ts>
constexpr bool is_lifetime_aware<std::variant<Ts...>> =
    (is_lifetime_aware<Ts> && ...);

template <class T, std::size_t N>
constexpr bool is_sendable<std::array<T, N>> = is_sendable<T>;
template <class T, std::size_t N>
constexpr bool is_lifetime_aware<std::array<T, N>> = is_lifetime_aware<T>;

template <>
inline constexpr bool is_synchronizable<std::stop_token> = true;
template <>
inline constexpr bool is_synchronizable<std::stop_source> = true;

template <>
inline constexpr bool is_lifetime_aware<std::stop_token> = true;
template <>
inline constexpr bool is_lifetime_aware<std::stop_source> = true;

}
