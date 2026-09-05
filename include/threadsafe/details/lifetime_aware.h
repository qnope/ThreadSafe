#pragma once

#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T> struct is_unsafe_lifetime_aware : std::false_type {};

template <class T>
constexpr bool is_unsafe_lifetime_aware_v = is_unsafe_lifetime_aware<T>::value;

inline consteval bool is_unsafe_lifetime_aware_type(std::meta::info type) {
  return detail::trait_value(^^is_unsafe_lifetime_aware_v, type);
}

namespace detail {
consteval bool diagnose_is_lifetime_aware(std::meta::info type);
}

template <class T> struct is_lifetime_aware {
  static constexpr bool value = detail::diagnose_is_lifetime_aware(^^T);
};

template <class T>
constexpr bool is_lifetime_aware_v =
    detail::assert_queryable_type<T>() && is_lifetime_aware<T>::value;

template <class T>
concept lifetime_aware = is_lifetime_aware_v<T>;

inline consteval bool is_lifetime_aware_type(std::meta::info type) {
  return detail::trait_value(^^is_lifetime_aware_v, type);
}

namespace detail {

template <class T> consteval bool pointee_is_lifetime_aware() {
  using pointee = typename T::element_type;
  return is_lifetime_aware_v<pointee> && dynamic_type_is_known<pointee>;
}

} // namespace detail

template <detail::smart_pointer T>
struct is_lifetime_aware<T>
    : std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};

namespace detail {

inline consteval bool diagnose_is_lifetime_aware(std::meta::info type) {
  if (const auto unqualified = remove_cv(type); unqualified != type)
    return is_lifetime_aware_type(unqualified);

  if (is_unsafe_lifetime_aware_type(type))
    return true;

  if (is_function_type(remove_pointer(type)))
    return true;

  if (is_reference_type(type) || is_pointer_type(type))
    return false;

  if (is_array_type(type))
    return is_lifetime_aware_type(remove_all_extents(type));

  if (extract<bool>(substitute(^^std::ranges::borrowed_range, {
                                                                  type})))
    return false;

  if (is_scalar_type(type))
    return true;

  if (!is_walkable_type(type))
    return false;

  return all_bases_and_members(type, is_lifetime_aware_type);
}

} // namespace detail

} // namespace threadsafe
