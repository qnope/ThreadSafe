#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <meta>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

namespace detail {

inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,
    ^^std::deque,
    ^^std::list,
    ^^std::forward_list,
    ^^std::basic_string,
    ^^std::map,
    ^^std::multimap,
    ^^std::set,
    ^^std::multiset,
    ^^std::unordered_map,
    ^^std::unordered_multimap,
    ^^std::unordered_set,
    ^^std::unordered_multiset,
    ^^std::pair,
    ^^std::tuple,
    ^^std::optional,
    ^^std::variant,
    ^^std::array,
};

inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
  type = dealias(type);
  return has_template_arguments(type) &&
         std::ranges::contains(allowed_std_wrappers, template_of(type));
}

template <class T>
concept std_wrapper = is_allowed_std_wrapper(^^T);

inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
  std::vector<std::meta::info> wrapped;
  const bool wrapper_is_const = is_const(type);

  for (auto argument : template_arguments_of(dealias(type)))
    if (is_type(argument))
      wrapped.push_back(wrapper_is_const ? add_const(remove_cv(argument))
                                         : remove_cv(argument));

  return wrapped;
}

inline consteval bool all_wrapped_types(std::meta::info type,
                                        bool (*question)(std::meta::info)) {
  for (auto wrapped : wrapped_types_of(type))
    if (!question(wrapped))
      return false;

  return true;
}

} // namespace detail

template <detail::std_wrapper T>
struct is_unsafe_sendable<T>
    : std::bool_constant<is_synchronizable_type(^^T) ||
                         detail::all_wrapped_types(^^T, is_sendable_type)> {};

template <detail::std_wrapper T>
struct is_unsafe_synchronizable<const T>
    : std::bool_constant<is_synchronizable_type(^^T) ||
                         detail::all_wrapped_types(^^const T,
                                                   is_synchronizable_type)> {};

template <detail::std_wrapper T>
struct is_unsafe_lifetime_aware<T>
    : std::bool_constant<detail::all_wrapped_types(^^T,
                                                   is_lifetime_aware_type)> {};

} // namespace threadsafe
