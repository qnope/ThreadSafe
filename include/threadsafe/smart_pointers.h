#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

// A unique_ptr owns its pointee, and the deleter travels with the pointer:
// sending it moves both. The array form owns elements, not an array object,
// so extents are stripped and sendability is that of the element type.
//
// dynamic_type_is_known blocks the type-erasure hole: through a pointer to a
// polymorphic, non-final base the owned object's real type is invisible, so a
// sendable base would otherwise smuggle a non-sendable derived object across
// the thread boundary. C++ has no `Box<dyn Trait + Send>` to carry the promise
// with the erased type; a hierarchy that really is sendable must say so with
// an explicit is_sendable specialization on the base.
template <class T, class D>
constexpr bool is_sendable<std::unique_ptr<T, D>> =
    is_sendable<std::remove_all_extents_t<T>> && is_sendable<D>
    && detail::dynamic_type_is_known<std::remove_all_extents_t<T>>;

// A unique_ptr keeps its pointee alive; a deleter that borrows does not.
template <class T, class D>
constexpr bool is_lifetime_aware<std::unique_ptr<T, D>> =
    is_lifetime_aware<std::remove_all_extents_t<T>> && is_lifetime_aware<D>;

// Sending a shared_ptr shares the referent with the copies left behind, so
// the referent must be synchronizable — and under this library's Sync ⇒ Send
// deviation that alone suffices (like Rust's Arc, simplified).
//
// Two holes remain here, neither visible in the type: a stateful custom
// deleter is type-erased into the control block and runs on whichever thread
// drops the last reference, and for a polymorphic base the assertion
// is_synchronizable<Base> necessarily covers every derived type reached
// through it. Both are documented in docs/thread-safety-audit.md.
template <class T>
constexpr bool is_sendable<std::shared_ptr<T>> =
    is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>>;

// Locking a sent weak_ptr yields shared access to the referent: same rule as
// shared_ptr.
template <class T>
constexpr bool is_sendable<std::weak_ptr<T>> =
    is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>>;

// A reference_wrapper is a reference in value clothing: same rule as T&.
template <class T>
constexpr bool is_sendable<std::reference_wrapper<T>> =
    is_synchronizable<std::remove_cv_t<T>>;

}  // namespace threadsafe
