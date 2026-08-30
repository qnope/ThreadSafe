#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_synchronizable {};

template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};

template <class T>
constexpr TraitAnswer is_unsafe_synchronizable_v
    = detail::unsafe_answer<is_unsafe_synchronizable, T>();

inline consteval TraitAnswer
is_unsafe_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_synchronizable_v, type);
}

namespace detail {

template <class T>
consteval TraitAnswer diagnose_is_synchronizable() {
    if (const auto vouched = is_unsafe_synchronizable_v<T>; vouched.answered)
        return vouched;

    return "is_unsafe_synchronizable<T> is opt-in: specialize it to vouch for "
           "a type that carries its own synchronization";
}

}

template <class T>
struct is_synchronizable {
    static constexpr TraitAnswer value = detail::diagnose_is_synchronizable<T>();
};

template <class T>
constexpr TraitAnswer is_synchronizable_v = is_synchronizable<T>::value;

template <class T, std::size_t N>
struct is_synchronizable<T[N]> : is_synchronizable<T> {};
template <class T>
struct is_synchronizable<T[]> : is_synchronizable<T> {};

inline consteval TraitAnswer is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable_v, type);
}

}
