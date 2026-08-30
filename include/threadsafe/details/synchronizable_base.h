#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

// The one way to say that a T may be used from several threads at once. The
// primary is empty: specializing it is what claims the type, and the claim is
// final, whether it says yes or no.
//
// Everything the library knows about a concrete type is written here rather
// than on is_synchronizable, so that the word `unsafe` appears wherever
// knowledge is asserted instead of proved.
template <class T>
struct is_unsafe_synchronizable {};

// Vouched for as writable from several threads at once, so also as readable.
// An unclaimed T leaves the base without a `value`, so the const form is
// unclaimed in turn — the empty primary propagates through inheritance.
template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};

template <class T>
constexpr TraitAnswer is_unsafe_synchronizable_v
    = detail::unsafe_answer<is_unsafe_synchronizable, T>();

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_unsafe_synchronizable_v<T>, for code written on the reflection
// side.
inline consteval TraitAnswer
is_unsafe_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_synchronizable_v, type);
}

namespace detail {

// is_synchronizable<T> has no structural definition to fall back on: no walk
// over members can show that concurrent writes are safe. Someone has to say so.
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

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_synchronizable_v<T>, for code written on the reflection side.
inline consteval TraitAnswer is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable_v, type);
}

}
