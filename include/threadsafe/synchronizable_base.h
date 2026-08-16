#pragma once

#include <meta>

#include <threadsafe/utils.h>

namespace threadsafe {

template <class T>
constexpr bool is_synchronizable = false;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_synchronizable<T>, for code written on the reflection side.
inline consteval bool is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable, type);
}

}

#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)         \
    template <>                                              \
    inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
