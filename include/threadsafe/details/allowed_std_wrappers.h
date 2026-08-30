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
    ^^std::vector,        ^^std::deque,             ^^std::list,
    ^^std::forward_list,  ^^std::basic_string,      ^^std::map,
    ^^std::multimap,      ^^std::set,               ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset,
    ^^std::pair,          ^^std::tuple,             ^^std::optional,
    ^^std::variant,       ^^std::array,
};

inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type)
        && std::ranges::contains(allowed_std_wrappers,
                                 std::meta::template_of(type));
}

template <class T>
concept std_wrapper = is_allowed_std_wrapper(^^T);

inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
    std::vector<std::meta::info> wrapped;
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type)))
        if (std::meta::is_type(argument))
            wrapped.push_back(std::meta::remove_cv(argument));
    return wrapped;
}

inline consteval TraitAnswer std_wrapper_is_sendable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return {};

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_sendable_type(wrapped))
            return "wraps a type that is not sendable";

    return {};
}

inline consteval TraitAnswer
std_wrapper_is_const_synchronizable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return {};

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_synchronizable_type(std::meta::add_const(wrapped)))
            return "wraps a type that is not readable from several threads "
                      "at once";

    return {};
}

inline consteval TraitAnswer
std_wrapper_is_lifetime_aware(std::meta::info type) {
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_lifetime_aware_type(wrapped))
            return "wraps a type that borrows instead of keeping its data "
                      "alive";

    return {};
}

}

template <detail::std_wrapper T>
struct is_unsafe_sendable<T> {
    static constexpr TraitAnswer value = detail::std_wrapper_is_sendable(^^T);
};

template <detail::std_wrapper T>
struct is_unsafe_synchronizable<const T> {
    static constexpr TraitAnswer value = detail::std_wrapper_is_const_synchronizable(^^T);
};

template <detail::std_wrapper T>
struct is_unsafe_lifetime_aware<T> {
    static constexpr TraitAnswer value = detail::std_wrapper_is_lifetime_aware(^^T);
};

}
