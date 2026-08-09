#pragma once

namespace threadsafe {

// True if a T may be used from multiple threads at the same time (≈ Rust Sync).
// Opt-in: specialize on the cv-unqualified, non-reference type. Queries must
// decay T first; cv-qualified queries are not forwarded.
//
// This header holds only the primary template, so sendable.h can build on it;
// the std::atomic rules (which need is_sendable) live in synchronizable.h.
template <class T>
constexpr bool is_synchronizable = false;

}  // namespace threadsafe

// Asserting that a type is synchronizable is an UNCHECKED promise: nothing in
// the library can verify it, exactly like Rust's `unsafe impl Sync`. Prefer
// this macro over specializing the variable template directly, so that every
// such promise in a codebase can be found with a single grep.
//
//   THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(MyMutexGuardedThing);
//
// Note that the promise covers the whole hierarchy: asserting it for a
// polymorphic base also blesses every derived type reached through a
// base pointer, since the dynamic type is invisible to the traits.
// `inline` matters: a full specialization of a variable template is a
// variable definition, not a template, so without it every translation unit
// that includes the header emits its own and the link fails.
#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)         \
    template <>                                              \
    inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
