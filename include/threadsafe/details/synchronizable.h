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
struct is_synchronizable<F> {
    static constexpr TraitAnswer value = {};
};

template <class T>
struct is_synchronizable<std::atomic<T>> : is_sendable<T> {};

namespace detail {
consteval TraitAnswer diagnose_is_const_synchronizable(std::meta::info type);
}

// Thread-safe read: a const T may be read from several threads at once. Full
// synchronizability implies it; otherwise the same structural guard as
// is_sendable applies, and every subobject reachable through the const must be
// read-safe in turn — a mutable member is writable through const and needs the
// full trait, and const behind an indirection is never trusted: the pointee
// may have been reached through a non-const alias at origin.
template <class T>
struct is_synchronizable<const T> {
    static constexpr TraitAnswer value
        = detail::diagnose_is_const_synchronizable(^^T);
};

// The const array forms exist because <const T> above matches a const array
// and would otherwise tie with the <T[N]> rule of synchronizable_base.h.
template <class T, std::size_t N>
struct is_synchronizable<const T[N]> : is_synchronizable<const T> {};
template <class T>
struct is_synchronizable<const T[]> : is_synchronizable<const T> {};

namespace detail {

inline consteval TraitAnswer
diagnose_is_const_synchronizable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    type = remove_cv(type);

    // Vouched for as read-only — the opt-in the const question has of its own,
    // beside the one full synchronizability carries.
    if (trait_value(^^is_unsafe_synchronizable_v, add_const(type)))
        return {};

    if (is_synchronizable_type(type))
        return {};

    // A pointee's const is a view restriction, not an object property — the
    // object may be written through another alias, so the full trait is asked.
    if (is_pointer_type(type)) {
        if (!is_synchronizable_type(remove_cv(remove_pointer(type))))
            return "is a pointer: the const stops at it — the pointee may "
                      "be written through another alias, so the pointee must "
                      "be synchronizable itself";
        return {};
    }

    if (is_array_type(type)) {
        const auto element = remove_cv(remove_extent(type));
        if (!is_synchronizable_type(add_const(element)))
            return "has an element type that is not readable from several "
                      "threads at once";
        return {};
    }

    if (is_scalar_type(type))
        return {};

    if (is_void_type(type))
        return "void holds no value to read";

    if (!is_class_type(type) && !is_union_type(type))
        return "is not a scalar, class or union type — "
                  "is_synchronizable<const T> supports no others";

    if (!is_complete_type(type))
        return "is incomplete — is_synchronizable<const T> needs a complete "
                  "type; specialize is_synchronizable for a type holding a "
                  "pointer to an incomplete type (the pimpl idiom)";

    if (const auto answer = is_default_type(type); !answer)
        return answer;

    if (has_unreflectable_state(type))
        return "holds state reflection cannot see (a closure type with "
                  "captures); specialize is_synchronizable to state the "
                  "intent";

    for (info base : bases_of(type, context))
        if (!is_synchronizable_type(add_const(type_of(base))))
            return "a base class is not readable from several threads at "
                      "once";

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = type_of(member);

        // mutable defeats const: this member is written through a const
        // reference, so it needs the full (write-safe) trait.
        if (is_mutable_member(member)) {
            if (!is_synchronizable_type(remove_cv(member_type)))
                return "a mutable member is written through a const "
                          "reference: its type must be fully synchronizable";
        }
        // A reference member's constness is unrelated to the referent's; the
        // referent may be shared and written through another alias.
        else if (is_reference_type(member_type)) {
            if (!is_synchronizable_type(remove_cvref(member_type)))
                return "a reference member stops the const: its referent "
                          "must be synchronizable itself";
        }
        // Ordinary value member: const propagates normally.
        else if (!is_synchronizable_type(add_const(member_type))) {
            return "a member is not readable from several threads at once";
        }
    }

    return {};
}

}

}
