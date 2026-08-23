#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

namespace detail {
consteval void diagnose_default_is_sendable(std::meta::info type);
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

// Same question as `static_assert(is_sendable_v<T>)`, but a failure names the
// subobject responsible instead of printing a bare false.
template <class T>
consteval void assert_sendable() {
    if (is_sendable_v<T>)
        return;

    detail::diagnose_default_is_sendable(^^T);

    throw std::meta::exception(
        u8"is_sendable is specialized to false for this type", ^^T);
}

namespace detail {

template <class T>
constexpr bool dynamic_type_is_known =
    !std::is_polymorphic_v<T> || std::is_final_v<T>;

// The trait itself, phrased so that "no" carries its reason. Returning means
// yes; a std::meta::exception carries the reason it is no — default_is_sendable
// catches it to answer false, while assert_* lets it escape so the reason
// becomes the diagnostic.
inline consteval void diagnose_default_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type) {
        if (!is_sendable_type(unqualified))
            reject(type, u8"is not sendable");
        return;
    }

    // Reached only from assert_sendable: the trait itself answers these through
    // its own specializations and never lands on the primary template.
    if (is_pointer_type(type) || is_reference_type(type))
        throw exception(
            u8"sending a reference or a pointer shares its referent with the "
            u8"other thread, so the referent must be synchronizable — and "
            u8"synchronizability is opt-in",
            type);

    if (is_array_type(type)) {
        if (!is_sendable_type(remove_cv(remove_extent(type))))
            reject(type, u8"has an element type that is not sendable");
        return;
    }

    if (is_synchronizable_type(type) || is_scalar_type(type))
        return;

    if (is_void_type(type))
        reject(type, u8"holds no value to send");

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

    if (!has_only_default_copy_move_destroy(type))
        reject(type,
               u8"has a user-written copy, move or destructor — or a template "
               u8"that may be selected as one — which can share state the "
               u8"members do not show; specialize is_sendable to state the "
               u8"intent");

    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); specialize is_sendable to state the intent");

    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            reject(base, u8"is not sendable");

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_sendable_type(remove_cv(type_of(member))))
            reject(member, u8"is not sendable");
}

inline consteval bool default_is_sendable(std::meta::info type) {
    try {
        diagnose_default_is_sendable(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}

}

}
