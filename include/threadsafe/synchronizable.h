#pragma once

#include <atomic>
#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/sendable.h>
#include <threadsafe/synchronizable_base.h>

namespace threadsafe {

template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
constexpr bool is_synchronizable<F> = true;

template <class T>
constexpr bool is_synchronizable<std::atomic<T>> = is_sendable<T>;

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
constexpr bool is_synchronizable<const T> =
    detail::default_is_const_synchronizable(^^T);

// Owned storage follows its element, like the is_sendable array rule; the
// const forms exist because <const T> and <T[N]> would otherwise be ambiguous
// for a const array.
template <class T, std::size_t N>
constexpr bool is_synchronizable<T[N]> = is_synchronizable<T>;
template <class T>
constexpr bool is_synchronizable<T[]> = is_synchronizable<T>;
template <class T, std::size_t N>
constexpr bool is_synchronizable<const T[N]> = is_synchronizable<const T>;
template <class T>
constexpr bool is_synchronizable<const T[]> = is_synchronizable<const T>;

namespace detail {

inline consteval bool default_is_const_synchronizable(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    type = std::meta::remove_cv(type);

    if (is_synchronizable_type(type))
        return true;

    // A pointee's const is a view restriction, not an object property — the
    // object may be written through another alias, so the full trait is asked.
    // A function pointee is code, and code is synchronizable.
    if (std::meta::is_pointer_type(type))
        return is_synchronizable_type(
            std::meta::remove_cv(std::meta::remove_pointer(type)));

    if (std::meta::is_scalar_type(type))
        return true;

    if (std::meta::is_void_type(type))
        return false;

    if (!std::meta::is_class_type(type) && !std::meta::is_union_type(type))
        throw std::meta::exception(
            u8"is_synchronizable<const T> supports only scalar, class and "
            u8"union types", type);

    if (!std::meta::is_complete_type(type))
        throw std::meta::exception(
            u8"is_synchronizable<const T> requires a complete type — "
            u8"specialize is_synchronizable for types holding a pointer to an "
            u8"incomplete type (the pimpl idiom)", type);

    if (!has_only_default_copy_move_destroy(type)
        || has_unreflectable_state(type))
        return false;

    for (std::meta::info base : std::meta::bases_of(type, ctx))
        if (!is_synchronizable_type(
                std::meta::add_const(std::meta::type_of(base))))
            return false;

    for (std::meta::info member : std::meta::nonstatic_data_members_of(type, ctx)) {
        const auto member_type = std::meta::type_of(member);
        if (std::meta::is_mutable_member(member)) {
            if (!is_synchronizable_type(std::meta::remove_cv(member_type)))
                return false;
        } else if (std::meta::is_reference_type(member_type)) {
            if (!is_synchronizable_type(std::meta::remove_cv(
                    std::meta::remove_reference(member_type))))
                return false;
        } else if (!is_synchronizable_type(std::meta::add_const(member_type))) {
            return false;
        }
    }
    return true;
}

}

}
