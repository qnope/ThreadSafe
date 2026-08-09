#pragma once

#include <type_traits>

#include <threadsafe/synchronizable.h>

namespace threadsafe {

namespace detail {
template <class F>
consteval bool default_is_safe_callable();
}

// True if a F may be invoked from multiple threads at the same time while
// shared between them. Safety of the object only, not invocability: emptiness
// means there is no per-object state to race on, even through a mutable
// operator().
//
// Emptiness is not statelessness. Static data members do not count toward
// std::is_empty_v, so an empty functor may still own unlimited shared mutable
// state — this trait cannot see through to any global, static or thread_local
// an operator() might touch, the same type-level limitation as the other
// traits. Treat it as "has no per-object state", not as a safety proof.
template <class F>
constexpr bool is_safe_callable = detail::default_is_safe_callable<F>();

// Code is immutable: invoking through a shared function pointer is safe.
template <function_type F>
constexpr bool is_safe_callable<F*> = true;

template <class F>
concept safe_callable = is_safe_callable<F>;

namespace detail {

template <class F>
consteval bool default_is_safe_callable() {
    // Forward cv-qualified queries, so that a const function pointer
    // (void (*const)(), the decayed type of a const callable) reaches the
    // function-pointer rule above instead of falling through to false.
    if constexpr (!std::is_same_v<F, std::remove_cv_t<F>>) {
        return is_safe_callable<std::remove_cv_t<F>>;
    } else {
        return is_synchronizable<F> || (std::is_class_v<F> && std::is_empty_v<F>);
    }
}

}  // namespace detail

}  // namespace threadsafe
