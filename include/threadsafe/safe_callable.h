#pragma once

#include <type_traits>

#include <threadsafe/synchronizable.h>

namespace threadsafe {

// True if a F may be invoked from multiple threads at the same time while
// shared between them. Safety of the object only, not invocability: emptiness
// means there is no per-object state to race on, even through a mutable
// operator(). Queries must decay F first; cv-qualified queries are not
// forwarded. This cannot see through to global or static state an operator()
// might touch — the same type-level limitation as the other traits.
template <class F>
constexpr bool is_safe_callable =
    is_synchronizable<F> || (std::is_class_v<F> && std::is_empty_v<F>);

// Code is immutable: invoking through a shared function pointer is safe.
template <function_type F>
constexpr bool is_safe_callable<F*> = true;

template <class F>
concept safe_callable = is_safe_callable<F>;

}  // namespace threadsafe
