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

template <class T>
constexpr bool is_sendable_v =
    detail::assert_queryable_type<T>() && is_sendable<T>::value;

template <class T>
concept sendable = is_sendable_v<T>;

inline consteval bool is_sendable_type(std::meta::info type) {
  return detail::trait_value(^^is_sendable_v, type);
}

namespace detail {

inline consteval bool diagnose_is_sendable(std::meta::info type) {
  if (const auto unqualified = remove_cv(type); unqualified != type)
    return is_sendable_type(unqualified);

  if (is_unsafe_sendable_type(type))
    return true;

  if (is_reference_type(type) || is_pointer_type(type))
    return is_synchronizable_type(remove_reference(remove_pointer(type)));

  if (is_array_type(type))
    return is_sendable_type(remove_all_extents(type));

  if (is_scalar_type(type) || is_synchronizable_type(type))
    return true;

  if (!is_walkable_type(type))
    return false;

  return all_bases_and_members(type, is_sendable_type);
}

} // namespace detail

} // namespace threadsafe
