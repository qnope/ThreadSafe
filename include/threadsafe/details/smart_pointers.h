#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

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

template <class T>
constexpr bool is_synchronizable<const std::default_delete<T>> = true;

template <class T, class D>
constexpr bool is_synchronizable<const std::unique_ptr<T, D>> =
    is_synchronizable<std::remove_all_extents_t<T>>
    && is_synchronizable<const D>;

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
