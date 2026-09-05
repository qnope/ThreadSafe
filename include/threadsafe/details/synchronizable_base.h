#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T> struct is_unsafe_synchronizable : std::false_type {};

template <class T>
struct is_unsafe_synchronizable<const T> : is_unsafe_synchronizable<T> {};

template <class T>
constexpr bool is_unsafe_synchronizable_v = is_unsafe_synchronizable<T>::value;

inline consteval bool is_unsafe_synchronizable_type(std::meta::info type) {
  return detail::trait_value(^^is_unsafe_synchronizable_v, type);
}

namespace detail {
consteval bool diagnose_is_synchronizable(std::meta::info type);
}

template <class T>
struct is_synchronizable
    : std::bool_constant<detail::diagnose_is_synchronizable(^^T)> {};

template <class T>
constexpr bool is_synchronizable_v =
    detail::assert_queryable_type<T>() && is_synchronizable<T>::value;

inline consteval bool is_synchronizable_type(std::meta::info type) {
  return detail::trait_value(^^is_synchronizable_v, type);
}

namespace detail {

inline consteval bool diagnose_is_synchronizable(std::meta::info type) {
  const auto context = std::meta::access_context::unchecked();

  if (is_unsafe_synchronizable_type(type))
    return true;

  if (is_function_type(remove_pointer(type)))
    return true;

  if (is_array_type(type))
    return is_synchronizable_type(remove_all_extents(type));

  if (!is_const(type))
    return false;

  if (is_pointer_type(type))
    return is_synchronizable_type(remove_cv(remove_pointer(type)));

  if (is_scalar_type(type))
    return true;

  if (!is_walkable_type(type))
    return false;

  for (auto base : bases_of(type, context))
    if (!is_synchronizable_type(add_const(type_of(base))))
      return false;

  for (auto member : nonstatic_data_members_of(type, context)) {
    const auto member_type = type_of(member);

    if (is_mutable_member(member)) {
      if (!is_synchronizable_type(remove_cv(member_type)))
        return false;
    } else if (is_reference_type(member_type)) {
      if (!is_synchronizable_type(remove_cvref(member_type)))
        return false;
    } else if (!is_synchronizable_type(add_const(member_type))) {
      return false;
    }
  }

  return true;
}

} // namespace detail

} // namespace threadsafe
