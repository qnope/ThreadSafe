#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

namespace detail {
consteval bool default_is_sendable(std::meta::info type);
}

template <class T>
struct is_sendable : std::bool_constant<detail::default_is_sendable(^^T)> {};

template <class T>
constexpr bool is_sendable_v = is_sendable<T>::value;

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
concept sendable = is_sendable_v<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_sendable_v<T>, for code written on the reflection side.
inline consteval bool is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable_v, type);
}

namespace detail {

template <class T>
constexpr bool dynamic_type_is_known =
    !std::is_polymorphic_v<T> || std::is_final_v<T>;

inline consteval bool default_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_sendable_type(unqualified);

    if (is_synchronizable_type(type) || is_scalar_type(type))
        return true;

    if (is_void_type(type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        throw exception(
            u8"is_sendable<T> supports only scalar, class and union types",
            type);

    if (!is_complete_type(type))
        throw exception(
            u8"is_sendable<T> requires a complete type — specialize is_sendable "
            u8"for types holding a pointer to an incomplete type (the pimpl "
            u8"idiom)",
            type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            return false;

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_sendable_type(remove_cv(type_of(member))))
            return false;

    return true;
}

}

}
