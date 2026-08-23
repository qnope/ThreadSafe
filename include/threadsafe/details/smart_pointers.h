#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_sendable<std::default_delete<T>> : std::true_type {};

template <class T, class D>
struct is_sendable<std::unique_ptr<T, D>>
    : std::bool_constant<
          is_sendable_v<std::remove_all_extents_t<T>> && is_sendable_v<D>
          && detail::dynamic_type_is_known<std::remove_all_extents_t<T>>> {};

template <class T, class D>
struct is_lifetime_aware<std::unique_ptr<T, D>>
    : std::bool_constant<is_lifetime_aware_v<std::remove_all_extents_t<T>>
                         && is_lifetime_aware_v<D>> {};

template <class T>
struct is_sendable<std::shared_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_sendable<std::weak_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_sendable<std::reference_wrapper<T>>
    : is_synchronizable<std::remove_cv_t<T>> {};

template <class T>
struct is_synchronizable<const std::default_delete<T>> : std::true_type {};

template <class T, class D>
struct is_synchronizable<const std::unique_ptr<T, D>>
    : std::bool_constant<is_synchronizable_v<std::remove_all_extents_t<T>>
                         && is_synchronizable_v<const D>> {};

template <class T>
struct is_synchronizable<const std::shared_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::weak_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::reference_wrapper<T>>
    : is_synchronizable<std::remove_cv_t<T>> {};

}
