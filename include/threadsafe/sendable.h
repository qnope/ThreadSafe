#pragma once

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

template <class T>
concept sendable = is_sendable<T>;

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
        static_assert(requires { sizeof(T); },
                      "is_sendable<T> requires a complete type");
        return has_no_user_provided_copy_move(^^T)
            && all_bases_and_members_sendable<T>();
    }
}

}  // namespace detail

}  // namespace threadsafe
