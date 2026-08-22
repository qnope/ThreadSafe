#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/synchronizable_base.h>
#include <threadsafe/utils.h>

namespace threadsafe {

namespace detail {
consteval bool default_is_sendable(std::meta::info type);
}

template <class T>
constexpr bool is_sendable = detail::default_is_sendable(^^T);

template <class T>
constexpr bool is_sendable<T&> = is_synchronizable<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_sendable<T&&> = is_synchronizable<std::remove_cv_t<T>>;

template <class T>
constexpr bool is_sendable<T*> = is_synchronizable<std::remove_cv_t<T>>;

template <class T, std::size_t N>
constexpr bool is_sendable<T[N]> = is_sendable<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_sendable<T[]> = is_sendable<std::remove_cv_t<T>>;

template <class T>
concept sendable = is_sendable<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_sendable<T>, for code written on the reflection side.
inline consteval bool is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable, type);
}

namespace detail {

template <class T>
constexpr bool dynamic_type_is_known =
    !std::is_polymorphic_v<T> || std::is_final_v<T>;

inline consteval bool default_is_sendable(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    const auto unqualified = std::meta::remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_sendable_type(unqualified);

    if (is_synchronizable_type(type) || std::meta::is_scalar_type(type))
        return true;

    if (std::meta::is_void_type(type))
        return false;

    if (!std::meta::is_class_type(type) && !std::meta::is_union_type(type))
        throw std::meta::exception(
            u8"is_sendable<T> supports only scalar, class and union types",
            type);

    if (!std::meta::is_complete_type(type))
        throw std::meta::exception(
            u8"is_sendable<T> requires a complete type — specialize is_sendable "
            u8"for types holding a pointer to an incomplete type (the pimpl "
            u8"idiom)",
            type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (std::meta::info b : std::meta::bases_of(type, ctx))
        if (!is_sendable_type(std::meta::type_of(b)))
            return false;

    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (!is_sendable_type(std::meta::remove_cv(std::meta::type_of(m))))
            return false;

    return true;
}

}

}

#include <threadsafe/containers.h>
#include <threadsafe/smart_pointers.h>
#include <threadsafe/synchronizable.h>
#include <threadsafe/vocabulary.h>
