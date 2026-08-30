#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_synchronizable {
    static constexpr TraitAnswer value = "is_unsafe_synchronizable<T> is opt-in: specialize it, or use "
              "THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE";
};

template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};

template <class T>
constexpr TraitAnswer is_unsafe_synchronizable_v
    = is_unsafe_synchronizable<T>::value;

template <class T>
struct is_synchronizable {
    static constexpr TraitAnswer value = is_unsafe_synchronizable_v<T>;
};

template <class T>
constexpr TraitAnswer is_synchronizable_v = is_synchronizable<T>::value;

template <class T, std::size_t N>
struct is_synchronizable<T[N]> : is_synchronizable<T> {};
template <class T>
struct is_synchronizable<T[]> : is_synchronizable<T> {};

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_synchronizable_v<T>, for code written on the reflection side.
inline consteval TraitAnswer is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable_v, type);
}

}

#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)                \
    template <>                                                     \
    struct threadsafe::is_unsafe_synchronizable<__VA_ARGS__> {      \
        static constexpr threadsafe::TraitAnswer value = {};        \
    };
