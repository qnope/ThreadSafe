#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/synchronizable_base.h>

namespace threadsafe {

namespace detail {
template <class T>
consteval bool default_is_sendable();
}

// True if a T may be sent from one thread to another (≈ Rust Send).
// Opt-in override: specialize on the cv-unqualified, non-reference type.
template <class T>
constexpr bool is_sendable = detail::default_is_sendable<T>();

// Sending a reference means sharing the referent.
template <class T>
constexpr bool is_sendable<T&> = is_synchronizable<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_sendable<T&&> = is_synchronizable<std::remove_cv_t<T>>;

// Sending a pointer shares the pointee, exactly like a reference. Function
// pointers are covered too: function types are synchronizable.
template <class T>
constexpr bool is_sendable<T*> = is_synchronizable<std::remove_cv_t<T>>;

// An array is owned storage, not a borrow: sending the object sends every
// element, so the element rule applies. Without this, the default below would
// reach its class/union assertion for any type holding a C array — which is
// std::array, std::mutex, std::function, std::any, and every struct with a
// fixed-size buffer.
template <class T, std::size_t N>
constexpr bool is_sendable<T[N]> = is_sendable<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_sendable<T[]> = is_sendable<std::remove_cv_t<T>>;

template <class T>
concept sendable = is_sendable<T>;

namespace detail {

// The traits describe the static type. Through a pointer to a polymorphic,
// non-final base the dynamic type is invisible, so a sendable base says
// nothing about the object actually owned. There is no equivalent of Rust's
// `Box<dyn Trait + Send>` to carry the promise along with the erased type.
template <class T>
constexpr bool dynamic_type_is_known =
    !std::is_polymorphic_v<T> || std::is_final_v<T>;

}  // namespace detail

namespace detail {

consteval bool is_copy_move_constructor_assignment(std::meta::info m) {
    return std::meta::is_copy_constructor(m) 
        || std::meta::is_move_constructor(m)
        || std::meta::is_copy_assignment(m)
        || std::meta::is_move_assignment(m);
}

// "Has no user-provided copy/move constructor or assignment operator".
// Implicitly-declared special members count as defaulted (core language), so
// plain aggregates pass. A deleted copy/move member does not block: deleting
// an operation cannot introduce sharing, and it keeps explicitly deleted
// copy members (the move-only idiom) consistent with the implicit deletion
// triggered by defaulted move members. Only a user-provided member — whose
// body could share state with the source object — blocks default
// sendability; users can opt back in by specializing is_sendable.
consteval bool has_no_user_provided_copy_move(std::meta::info type) {
    auto const ctx = std::meta::access_context::unchecked();
    for (std::meta::info m : std::meta::members_of(type, ctx)) {
        if (!is_copy_move_constructor_assignment(m))
            continue;

        if (!std::meta::is_defaulted(m) && !std::meta::is_deleted(m))
            return false;
    }
    return true;
}

// Re-enters the is_sendable customization point for every base class and
// non-static data member, so user specializations are honored during
// recursion.
// Termination: recursion only descends through bases and members, which are
// complete types strictly inside T. A cycle back to T must go through a
// pointer or reference, and those stop immediately at the pointer and
// reference specializations above - no visited-set machinery is needed.
template <class T>
consteval bool all_bases_and_members_sendable() {
    constexpr auto ctx = std::meta::access_context::unchecked();
    
    template for (constexpr std::meta::info b :
                  std::define_static_array(std::meta::bases_of(^^T, ctx)))
    {
        using B = [:std::meta::type_of(b):];
        if (!is_sendable<B>)
            return false;
    }

    template for (constexpr std::meta::info m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx)))
    {
        using M = [:std::meta::type_of(m):];
        if (!is_sendable<std::remove_cv_t<M>>)
            return false;
    }
    
    return true;
}

template <class T>
consteval bool default_is_sendable() {
    if constexpr (!std::is_same_v<T, std::remove_cv_t<T>>) {
        return is_sendable<std::remove_cv_t<T>>;
    } else if constexpr (is_synchronizable<T>) {
        return true;
    } else if constexpr (std::is_scalar_v<T>) {
        // Arithmetic types, enums, nullptr_t, member (function) pointers.
        // Object and function pointers never reach here: the is_sendable<T*>
        // specialization handles them.
        return true;
    } else if constexpr (std::is_void_v<T>) {
        return false;
    } else {
        static_assert(std::is_class_v<T> || std::is_union_v<T>,
                      "is_sendable<T> supports only scalar, class and union types");
        // Deliberately a hard error rather than a conservative false: whether
        // a type is complete depends on the point of instantiation, so
        // answering from completeness would make is_sendable<T> differ between
        // translation units — the very hazard the bottom includes below exist
        // to prevent. A pimpl'd type must state its own answer:
        //     template <> constexpr bool threadsafe::is_sendable<Widget> = ...;
        static_assert(requires { sizeof(T); },
                      "is_sendable<T> requires a complete type — specialize "
                      "is_sendable for types holding a pointer to an "
                      "incomplete type (the pimpl idiom)");
        return has_no_user_provided_copy_move(^^T)
            && all_bases_and_members_sendable<T>();
    }
}

}  // namespace detail

}  // namespace threadsafe

// A variable template is a customization point whose value is fixed at the
// point of instantiation, so a translation unit that saw only *some* of the
// specializations computes a DIFFERENT answer for the same type — silently,
// and without any diagnostic at link time. Pulling the full set in here makes
// every entry point into the library agree. The nested includes are no-ops
// while this header is still in flight (#pragma once), and nothing is
// instantiated during header processing, so the cycle is benign.
#include <threadsafe/containers.h>
#include <threadsafe/smart_pointers.h>
#include <threadsafe/synchronizable.h>
#include <threadsafe/vocabulary.h>
