#pragma once

#include <cstddef>
#include <meta>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
constexpr bool is_synchronizable = false;

// Owned storage follows its element, like the is_sendable array rule. It sits
// with the primary template because it is pure forwarding: it composes with
// whatever answer T has, including one a TU adds with
// THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE.
template <class T, std::size_t N>
constexpr bool is_synchronizable<T[N]> = is_synchronizable<T>;
template <class T>
constexpr bool is_synchronizable<T[]> = is_synchronizable<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_synchronizable<T>, for code written on the reflection side.
inline consteval bool is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable, type);
}

}

#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)         \
    template <>                                              \
    inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
