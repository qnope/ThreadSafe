#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

// Stateless, and the object it deletes is the unique_ptr's business, not its
// own. It needs a rule only because its converting constructor is a template,
// which the default reads as a constructor that could stand in for the copy.
template <class T>
constexpr bool is_sendable<std::default_delete<T>> = true;

template <class T, class D>
constexpr bool is_sendable<std::unique_ptr<T, D>> =
    is_sendable<std::remove_all_extents_t<T>> && is_sendable<D>
    && detail::dynamic_type_is_known<std::remove_all_extents_t<T>>;

template <class T, class D>
constexpr bool is_lifetime_aware<std::unique_ptr<T, D>> =
    is_lifetime_aware<std::remove_all_extents_t<T>> && is_lifetime_aware<D>;

template <class T>
constexpr bool is_sendable<std::shared_ptr<T>> =
    is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>>;

template <class T>
constexpr bool is_sendable<std::weak_ptr<T>> =
    is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>>;

template <class T>
constexpr bool is_sendable<std::reference_wrapper<T>> =
    is_synchronizable<std::remove_cv_t<T>>;

// Same reason as the is_sendable rule above: the converting-constructor
// template blocks the structural default.
template <class T>
constexpr bool is_synchronizable<const std::default_delete<T>> = true;

// Owned storage, like a container's elements: the element keeps its own cv
// through get() const — a const unique_ptr<const T> hands readers const T*
// over a pointee nothing else aliases, the same assumption its is_sendable
// rule already makes.
template <class T, class D>
constexpr bool is_synchronizable<const std::unique_ptr<T, D>> =
    is_synchronizable<std::remove_all_extents_t<T>>
    && is_synchronizable<const D>;

// Shared views: another handle to the same object may be a shared_ptr<T>, so
// the pointee's const is never trusted — copying the const handle itself is
// refcount-atomic-safe, the pointee is the whole question.
template <class T>
constexpr bool is_synchronizable<const std::shared_ptr<T>> =
    is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>>;

template <class T>
constexpr bool is_synchronizable<const std::weak_ptr<T>> =
    is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>>;

template <class T>
constexpr bool is_synchronizable<const std::reference_wrapper<T>> =
    is_synchronizable<std::remove_cv_t<T>>;

}
