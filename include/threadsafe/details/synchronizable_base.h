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
consteval bool diagnose_is_const_synchronizable(std::meta::info type);
}

template <class T> struct is_synchronizable : is_unsafe_synchronizable<T> {};

template <class T>
struct is_synchronizable<const T>
    : std::disjunction<
          is_unsafe_synchronizable<const T>,
          std::bool_constant<detail::diagnose_is_const_synchronizable(
              ^^const T)>> {};

template <class T>
constexpr bool is_synchronizable_v = is_synchronizable<T>::value;

inline consteval bool is_synchronizable_type(std::meta::info type) {
  return detail::trait_value(^^is_synchronizable_v, type);
}

template <class T>
  requires(std::is_function_v<T>)
struct is_synchronizable<T> : std::true_type {};

template <class T>
  requires(std::is_array_v<T>)
struct is_synchronizable<const T>
    : is_synchronizable<const std::remove_all_extents_t<T>> {};

template <class T>
  requires(std::is_array_v<T>)
struct is_synchronizable<T> : is_synchronizable<std::remove_all_extents_t<T>> {
};

namespace detail {

inline consteval bool diagnose_is_const_synchronizable(std::meta::info type) {
  using namespace std::meta;

  const auto context = access_context::unchecked();
  type = remove_cv(type);

  if (is_pointer_type(type))
    return is_synchronizable_type(remove_cv(remove_pointer(type)));

  if (is_scalar_type(type))
    return true;

  if (!is_class_type(type) && !is_union_type(type))
    return false;

  if (!is_complete_type(type))
    return false;

  if (!is_default_type(type))
    return false;

  if (has_unreflectable_state(type))
    return false;

  for (info base : bases_of(type, context))
    if (!is_synchronizable_type(add_const(type_of(base))))
      return false;

  for (info member : nonstatic_data_members_of(type, context)) {
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
