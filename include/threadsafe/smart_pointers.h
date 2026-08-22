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

}
