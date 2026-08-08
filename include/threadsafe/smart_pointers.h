#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/sendable.h>

namespace threadsafe {

// A unique_ptr owns its pointee, and the deleter travels with the pointer:
// sending it moves both. The array form owns elements, not an array object,
// so extents are stripped and sendability is that of the element type.
template <class T, class D>
constexpr bool is_sendable<std::unique_ptr<T, D>> =
    is_sendable<std::remove_all_extents_t<T>> && is_sendable<D>;

// Sending a shared_ptr shares the referent with the copies left behind, so
// the referent must be synchronizable — and under this library's Sync ⇒ Send
// deviation that alone suffices (like Rust's Arc, simplified). A stateful
// custom deleter is type-erased into the control block and cannot be checked
// statically.
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
