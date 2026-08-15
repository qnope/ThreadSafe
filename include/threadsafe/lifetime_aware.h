#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

namespace threadsafe {

namespace detail {
template <class T>
consteval bool default_is_lifetime_aware();
}

template <class T>
constexpr bool is_lifetime_aware = detail::default_is_lifetime_aware<T>();

template <class T>
constexpr bool is_lifetime_aware<T&> = false;
template <class T>
constexpr bool is_lifetime_aware<T&&> = false;
template <class T>
constexpr bool is_lifetime_aware<T*> = false;

template <class F>
    requires std::is_function_v<F>
constexpr bool is_lifetime_aware<F*> = true;

template <class T, std::size_t N>
constexpr bool is_lifetime_aware<T[N]> = is_lifetime_aware<std::remove_cv_t<T>>;

template <class T>
constexpr bool is_lifetime_aware<std::reference_wrapper<T>> = false;

template <class T>
constexpr bool is_lifetime_aware<std::shared_ptr<T>> = true;
template <class T>
constexpr bool is_lifetime_aware<std::weak_ptr<T>> = true;

template <class T>
concept lifetime_aware = is_lifetime_aware<T>;

namespace detail {

template <class T>
consteval bool all_bases_and_members_lifetime_aware() {
    constexpr auto ctx = std::meta::access_context::unchecked();

    template for (constexpr std::meta::info b :
                  std::define_static_array(std::meta::bases_of(^^T, ctx)))
    {
        using B = [:std::meta::type_of(b):];
        if (!is_lifetime_aware<B>)
            return false;
    }

    template for (constexpr std::meta::info m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx)))
    {
        using M = [:std::meta::type_of(m):];
        if (!is_lifetime_aware<std::remove_cv_t<M>>)
            return false;
    }

    return true;
}

template <class T>
consteval bool default_is_lifetime_aware() {
    if constexpr (!std::is_same_v<T, std::remove_cv_t<T>>) {
        return is_lifetime_aware<std::remove_cv_t<T>>;
    } else if constexpr (std::ranges::borrowed_range<T>) {
        return false;
    } else if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
        static_assert(requires { sizeof(T); },
                      "is_lifetime_aware<T> requires a complete type");
        return all_bases_and_members_lifetime_aware<T>();
    } else {
        return true;
    }
}

}

}

#include <threadsafe/containers.h>
#include <threadsafe/smart_pointers.h>
#include <threadsafe/vocabulary.h>
