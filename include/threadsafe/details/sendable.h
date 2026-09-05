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

  const auto context = access_context::unchecked();

  if (const auto unqualified = remove_cv(type); unqualified != type)
    return is_sendable_type(unqualified);

  if (is_unsafe_sendable_type(type))
    return true;

  if (is_scalar_type(type) || is_synchronizable_type(type))
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
    if (!is_sendable_type(type_of(base)))
      return false;

  for (info member : nonstatic_data_members_of(type, context))
    if (!is_sendable_type(remove_cv(type_of(member))))
      return false;

  return true;
}

} // namespace detail

} // namespace threadsafe
