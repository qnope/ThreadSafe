#pragma once

#include <array>
#include <complex>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

// The everyday composites own their elements, so they are sendable and
// lifetime-aware exactly when every element is.
//
// These need explicit rules because libstdc++ declares constrained-but-
// semantically-defaulted special members for them, e.g.
//     constexpr pair& operator=(const pair&) requires _S_assignable<...>();
// which std::meta reports as user-provided. The reflection default therefore
// answers false for std::pair, std::tuple and std::optional — the value_type
// of every map and the most common composites in C++.

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

// std::array is an aggregate over a C array; the array rules in sendable.h and
// lifetime_aware.h already give the right answer, but stating it directly
// keeps the answer independent of the implementation's internals.
template <class T, std::size_t N>
constexpr bool is_sendable<std::array<T, N>> = is_sendable<T>;
template <class T, std::size_t N>
constexpr bool is_lifetime_aware<std::array<T, N>> = is_lifetime_aware<T>;

// std::complex holds its value in a compiler extension type (__complex__ T),
// which is neither scalar, class nor union, so the reflection default cannot
// classify it and would hard-error. The value is two T's with no indirection.
// Other extension types (GCC vector types) still need their own rule.
template <class T>
constexpr bool is_sendable<std::complex<T>> = is_sendable<T>;
template <class T>
constexpr bool is_lifetime_aware<std::complex<T>> = is_lifetime_aware<T>;

// [stoptoken.general] guarantees that concurrent calls to a stop_token's
// observers, and to stop_source::request_stop, are race-free — these types are
// synchronizable by specification, not by assertion. Both share ownership of a
// reference-counted stop state, so they keep it alive.
//
// This matters beyond completeness: std::jthread PREPENDS a stop_token to the
// argument list whenever the callable accepts one, and that injected argument
// never passes through the launcher's Args constraints. The launcher asserts
// these two values rather than assuming them.
template <>
inline constexpr bool is_synchronizable<std::stop_token> = true;
template <>
inline constexpr bool is_synchronizable<std::stop_source> = true;

template <>
inline constexpr bool is_lifetime_aware<std::stop_token> = true;
template <>
inline constexpr bool is_lifetime_aware<std::stop_source> = true;

}  // namespace threadsafe
