#pragma once

#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T> struct is_unsafe_sendable : std::false_type {};

template <class T>
constexpr bool is_unsafe_sendable_v = is_unsafe_sendable<T>::value;

inline consteval bool is_unsafe_sendable_type(std::meta::info type) {
  return detail::trait_value(^^is_unsafe_sendable_v, type);
}

namespace detail {
consteval bool diagnose_is_sendable(std::meta::info type);
}

template <class T>
struct is_sendable : std::bool_constant<detail::diagnose_is_sendable(^^T)> {};

template <class T> constexpr bool is_sendable_v = is_sendable<T>::value;

template <class T>
concept sendable = is_sendable_v<T>;

template <class T>
  requires(std::is_reference_v<T>)
struct is_sendable<T> : is_synchronizable<std::remove_reference_t<T>> {};

template <class T> struct is_sendable<T *> : is_synchronizable<T> {};

template <class T>
  requires(std::is_array_v<T>)
struct is_sendable<T> : is_sendable<std::remove_all_extents_t<T>> {};

inline consteval bool is_sendable_type(std::meta::info type) {
  return detail::trait_value(^^is_sendable_v, type);
}

namespace detail {

inline consteval bool diagnose_is_sendable(std::meta::info type) {
  using namespace std::meta;

  if (const auto unqualified = remove_cv(type); unqualified != type)
    return is_sendable_type(unqualified);

  if (is_unsafe_sendable_type(type))
    return true;

  if (is_scalar_type(type) || is_synchronizable_type(type))
    return true;

  if (!is_walkable_class(type))
    return false;

  return all_bases_and_members(type, is_sendable_type);
}

} // namespace detail

} // namespace threadsafe
