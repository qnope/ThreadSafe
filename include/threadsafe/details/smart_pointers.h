#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

namespace detail {

template <class T> consteval bool pointee_is_lifetime_aware() {
  const auto template_arguments = wrapped_types_of(^^T);

  return std::ranges::all_of(template_arguments, [](const auto argument) {
    return pointee_answer(argument, is_lifetime_aware_type);
  });
}

template <class T> consteval bool pointee_is_synchronizable() {
  return pointee_answer(^^std::remove_cv_t<std::remove_all_extents_t<T>>,
                        is_synchronizable_type);
}

} // namespace detail

template <typename T> struct is_smart_pointer : std::false_type {};

template <typename T>
struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

template <typename T>
struct is_smart_pointer<std::weak_ptr<T>> : std::true_type {};

template <typename T>
struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

template <typename T>
constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;

template <class T>
concept smart_pointer = is_smart_pointer<T>::value;

inline consteval bool is_smart_pointer_type(std::meta::info info) {
  return detail::trait_value(^^is_smart_pointer_v, info);
}

template <smart_pointer T>
struct is_unsafe_lifetime_aware<T>
    : std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};

template <class T>
struct is_unsafe_sendable<std::default_delete<T>> : std::true_type {};

template <class T>
struct is_unsafe_lifetime_aware<std::default_delete<T>> : std::true_type {};

template <class T>
struct is_unsafe_synchronizable<std::default_delete<T>> : std::true_type {};

template <class T, class D>
struct is_unsafe_sendable<std::unique_ptr<T, D>>
    : std::bool_constant<detail::pointee_answer(^^T, is_sendable_type) &&
                         is_sendable_v<D>> {};

template <class T>
struct is_unsafe_sendable<std::shared_ptr<T>>
    : std::bool_constant<detail::pointee_is_synchronizable<T>()> {};

template <class T>
struct is_unsafe_sendable<std::weak_ptr<T>>
    : std::bool_constant<detail::pointee_is_synchronizable<T>()> {};

template <class T>
struct is_unsafe_sendable<std::reference_wrapper<T>>
    : std::bool_constant<detail::pointee_is_synchronizable<T>()> {};

template <class T, class D>
struct is_unsafe_synchronizable<const std::unique_ptr<T, D>>
    : std::bool_constant<detail::pointee_answer(^^T, is_synchronizable_type) &&
                         is_synchronizable_v<const D>> {};

template <class T>
struct is_unsafe_synchronizable<const std::shared_ptr<T>>
    : std::bool_constant<detail::pointee_is_synchronizable<T>()> {};

template <class T>
struct is_unsafe_synchronizable<const std::weak_ptr<T>>
    : std::bool_constant<detail::pointee_is_synchronizable<T>()> {};

template <class T>
struct is_unsafe_synchronizable<const std::reference_wrapper<T>>
    : std::bool_constant<detail::pointee_is_synchronizable<T>()> {};

} // namespace threadsafe
