#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_synchronizable : std::false_type {};

template <class T>
constexpr bool is_synchronizable_v = is_synchronizable<T>::value;

template <class T, std::size_t N>
struct is_synchronizable<T[N]> : is_synchronizable<T> {};
template <class T>
struct is_synchronizable<T[]> : is_synchronizable<T> {};

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_synchronizable_v<T>, for code written on the reflection side.
inline consteval bool is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable_v, type);
}

}

#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)  \
    template <>                                       \
    struct threadsafe::is_synchronizable<__VA_ARGS__> : std::true_type {}
