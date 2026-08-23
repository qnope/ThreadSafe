#pragma once

#include <atomic>
#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable_base.h>

namespace threadsafe {

template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
struct is_synchronizable<F> : std::true_type {};

template <class T>
struct is_synchronizable<std::atomic<T>> : is_sendable<T> {};

namespace detail {
consteval bool default_is_const_synchronizable(std::meta::info type);
}

// Thread-safe read: a const T may be read from several threads at once. Full
// synchronizability implies it; otherwise the same structural guard as
// is_sendable applies, and every subobject reachable through the const must be
// read-safe in turn — a mutable member is writable through const and needs the
// full trait, and const behind an indirection is never trusted: the pointee
// may have been reached through a non-const alias at origin.
template <class T>
struct is_synchronizable<const T>
    : std::bool_constant<detail::default_is_const_synchronizable(^^T)> {};

// The const array forms exist because <const T> above matches a const array
// and would otherwise tie with the <T[N]> rule of synchronizable_base.h.
template <class T, std::size_t N>
struct is_synchronizable<const T[N]> : is_synchronizable<const T> {};
template <class T>
struct is_synchronizable<const T[]> : is_synchronizable<const T> {};

namespace detail {

inline consteval bool default_is_const_synchronizable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    type = remove_cv(type);

    if (is_synchronizable_type(type))
        return true;

    // A pointee's const is a view restriction, not an object property — the
    // object may be written through another alias, so the full trait is asked.
    // A function pointee is code, and code is synchronizable.
    if (is_pointer_type(type))
        return is_synchronizable_type(remove_cv(remove_pointer(type)));

    if (is_scalar_type(type))
        return true;

    if (is_void_type(type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        throw exception(
            u8"is_synchronizable<const T> supports only scalar, class and "
            u8"union types", type);

    if (!is_complete_type(type))
        throw exception(
            u8"is_synchronizable<const T> requires a complete type — "
            u8"specialize is_synchronizable for types holding a pointer to an "
            u8"incomplete type (the pimpl idiom)", type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_synchronizable_type(add_const(type_of(base))))
            return false;

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = type_of(member);
        if (is_mutable_member(member)) {
            // mutable defeats const: this member is writable through a const&, so it
            // needs the full (write-safe) trait, not the const one.
            if (!is_synchronizable_type(remove_cv(member_type)))
                return false;
        } else if (is_reference_type(member_type)) {
            // a reference member's constness is unrelated to the referent's; the
            // referent may be shared and mutated through another alias.
            if (!is_synchronizable_type(remove_cvref(member_type)))
                return false;
        } else if (!is_synchronizable_type(add_const(member_type))) {
            return false; // ordinary value member: const propagates normally.
        }
    }
    
    return true;
}

}

}
