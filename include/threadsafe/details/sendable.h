#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

// The one way to answer for a type the structural walk cannot read — a handle,
// a std::allocator, a std::vector. The primary is empty: specializing it is
// what claims the type, and the claim is final, whether it says yes or no.
//
// Everything the library knows about a concrete type is written here rather
// than on is_sendable, so that the word `unsafe` appears wherever knowledge is
// asserted instead of proved. is_sendable itself holds only its definition.
template <class T>
struct is_unsafe_sendable {};

template <class T>
constexpr TraitAnswer is_unsafe_sendable_v
    = detail::unsafe_answer<is_unsafe_sendable, T>();

inline consteval TraitAnswer is_unsafe_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_sendable_v, type);
}

namespace detail {
consteval TraitAnswer diagnose_is_sendable(std::meta::info type);
}

template <class T>
struct is_sendable {
    static constexpr TraitAnswer value = detail::diagnose_is_sendable(^^T);
};

template <class T>
constexpr TraitAnswer is_sendable_v = is_sendable<T>::value;

template <class T>
struct is_sendable<const T> : is_sendable<std::remove_const_t<T>> {};

template <class T>
struct is_sendable<T&> : is_synchronizable<std::remove_cv_t<T>> {};
template <class T>
struct is_sendable<T&&> : is_synchronizable<std::remove_cv_t<T>> {};

template <class T>
struct is_sendable<T*> : is_synchronizable<std::remove_cv_t<T>> {};

template <class T, std::size_t N>
struct is_sendable<T[N]> : is_sendable<std::remove_cv_t<T>> {};
template <class T>
struct is_sendable<T[]> : is_sendable<std::remove_cv_t<T>> {};

template <class T>
concept sendable = bool(is_sendable_v<T>);

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_sendable_v<T>, for code written on the reflection side.
inline consteval TraitAnswer is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable_v, type);
}

namespace detail {

// The structural default: a type is sendable when every base and every member
// is. A default-constructed answer means yes; otherwise it says why not.
inline consteval TraitAnswer diagnose_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();

    if (const auto vouched = is_unsafe_sendable_type(type); vouched.answered)
        return vouched;

    if (is_synchronizable_type(type) || is_scalar_type(type))
        return {};

    if (is_void_type(type))
        return "void holds no value to send";

    if (!is_class_type(type) && !is_union_type(type))
        return "is not a scalar, class or union type — is_sendable<T> "
                  "supports no others";

    if (!is_complete_type(type))
        return "is incomplete — is_sendable<T> needs a complete type; "
                  "specialize is_sendable for a type holding a pointer to an "
                  "incomplete type (the pimpl idiom)";

    if (const auto answer = is_default_type(type); !answer)
        return answer;

    if (has_unreflectable_state(type))
        return "holds state reflection cannot see (a closure type with "
                  "captures); specialize is_sendable to state the intent";

    for (info base : bases_of(type, context))
        if (const auto answer = is_sendable_type(type_of(base)); !answer)
            return "a base class is not sendable";

    for (info member : nonstatic_data_members_of(type, context))
        if (const auto answer = is_sendable_type(remove_cv(type_of(member)));
            !answer)
            return "a member is not sendable";

    return {};
}

}

}
