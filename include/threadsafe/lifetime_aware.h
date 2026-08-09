#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

namespace threadsafe {

namespace detail {
template <class T>
consteval bool default_is_lifetime_aware();
}

// True if a T owns its data or keeps its referent alive.
//
// Ownership is transitive, exactly like Rust's 'static bound: a struct holding
// a T* is no more an owner than the T* itself, so the default recurses through
// every base class and non-static data member. Borrowed ranges are false
// outright — a view is a borrow however it is packaged.
// Queries must decay T first; cv-qualified queries are not forwarded.
template <class T>
constexpr bool is_lifetime_aware = detail::default_is_lifetime_aware<T>();

// References and pointers do not keep their referent alive.
template <class T>
constexpr bool is_lifetime_aware<T&> = false;
template <class T>
constexpr bool is_lifetime_aware<T&&> = false;
template <class T>
constexpr bool is_lifetime_aware<T*> = false;

// ...except a pointer to code: functions have static storage duration, so a
// function pointer can never dangle. This mirrors the carve-outs function
// types already get in is_sendable and is_safe_callable, and without it a
// plain function cannot be handed to launch_task at all.
template <class F>
    requires std::is_function_v<F>
constexpr bool is_lifetime_aware<F*> = true;

// An array is owned storage: it keeps its elements alive.
template <class T, std::size_t N>
constexpr bool is_lifetime_aware<T[N]> = is_lifetime_aware<std::remove_cv_t<T>>;

template <class T>
constexpr bool is_lifetime_aware<std::reference_wrapper<T>> = false;

// A shared_ptr normally owns its pointee. The aliasing constructor and a
// no-op deleter both break that promise, and neither is visible in the type,
// so this rule is a genuine (documented) hole rather than a proof.
template <class T>
constexpr bool is_lifetime_aware<std::shared_ptr<T>> = true;
template <class T>
constexpr bool is_lifetime_aware<std::weak_ptr<T>> = true;

template <class T>
concept lifetime_aware = is_lifetime_aware<T>;

namespace detail {

// Re-enters the is_lifetime_aware customization point for every base class and
// non-static data member, so user and std specializations are honored during
// recursion.
// Termination: the same argument as is_sendable. Recursion only descends into
// complete types strictly inside T, and any cycle back to T must pass through
// a pointer or a reference, which stop at the specializations above.
template <class T>
consteval bool all_bases_and_members_lifetime_aware() {
    constexpr auto ctx = std::meta::access_context::unchecked();

    template for (constexpr std::meta::info b :
                  std::define_static_array(std::meta::bases_of(^^T, ctx)))
    {
        using B = [:std::meta::type_of(b):];
        if (!is_lifetime_aware<B>)
            return false;
    }

    template for (constexpr std::meta::info m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx)))
    {
        using M = [:std::meta::type_of(m):];
        if (!is_lifetime_aware<std::remove_cv_t<M>>)
            return false;
    }

    return true;
}

template <class T>
consteval bool default_is_lifetime_aware() {
    if constexpr (!std::is_same_v<T, std::remove_cv_t<T>>) {
        return is_lifetime_aware<std::remove_cv_t<T>>;
    } else if constexpr (std::ranges::borrowed_range<T>) {
        // Views over data they do not own.
        return false;
    } else if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
        static_assert(requires { sizeof(T); },
                      "is_lifetime_aware<T> requires a complete type");
        return all_bases_and_members_lifetime_aware<T>();
    } else {
        // Scalars other than object pointers (handled above), enums, member
        // pointers, functions. None of them borrow.
        return true;
    }
}

}  // namespace detail

}  // namespace threadsafe

// See the matching note at the bottom of sendable.h: the trait is only
// meaningful together with its full set of specializations, or translation
// units disagree silently.
#include <threadsafe/containers.h>
#include <threadsafe/smart_pointers.h>
#include <threadsafe/vocabulary.h>
