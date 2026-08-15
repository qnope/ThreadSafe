#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/synchronizable_base.h>
#include <threadsafe/utils.h>

namespace threadsafe {

namespace detail {
template <class T>
consteval bool default_is_sendable();
}

template <class T>
constexpr bool is_sendable = detail::default_is_sendable<T>();

template <class T>
constexpr bool is_sendable<T&> = is_synchronizable<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_sendable<T&&> = is_synchronizable<std::remove_cv_t<T>>;

template <class T>
constexpr bool is_sendable<T*> = is_synchronizable<std::remove_cv_t<T>>;

template <class T, std::size_t N>
constexpr bool is_sendable<T[N]> = is_sendable<std::remove_cv_t<T>>;
template <class T>
constexpr bool is_sendable<T[]> = is_sendable<std::remove_cv_t<T>>;

template <class T>
concept sendable = is_sendable<T>;

namespace detail {

template <class T>
constexpr bool dynamic_type_is_known =
    !std::is_polymorphic_v<T> || std::is_final_v<T>;

}

namespace detail {

template <class T>
consteval bool all_bases_and_members_sendable() {
    constexpr auto ctx = std::meta::access_context::unchecked();

    template for (constexpr std::meta::info b :
                  std::define_static_array(std::meta::bases_of(^^T, ctx)))
    {
        using B = [:std::meta::type_of(b):];
        if (!is_sendable<B>)
            return false;
    }

    template for (constexpr std::meta::info m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx)))
    {
        using M = [:std::meta::type_of(m):];
        if (!is_sendable<std::remove_cv_t<M>>)
            return false;
    }

    return true;
}

template <class T>
consteval bool default_is_sendable() {
    if constexpr (!std::is_same_v<T, std::remove_cv_t<T>>) {
        return is_sendable<std::remove_cv_t<T>>;
    } else if constexpr (is_synchronizable<T>) {
        return true;
    } else if constexpr (std::is_scalar_v<T>) {
        return true;
    } else if constexpr (std::is_void_v<T>) {
        return false;
    } else {
        static_assert(std::is_class_v<T> || std::is_union_v<T>,
                      "is_sendable<T> supports only scalar, class and union types");
        static_assert(std::meta::is_complete_type(^^T),
                      "is_sendable<T> requires a complete type — specialize "
                      "is_sendable for types holding a pointer to an "
                      "incomplete type (the pimpl idiom)");
        return has_only_default_copy_move_constructor_assignment(^^T)
            && !has_unreflectable_state(^^T)
            && all_bases_and_members_sendable<T>();
    }
}

}

}

#include <threadsafe/containers.h>
#include <threadsafe/smart_pointers.h>
#include <threadsafe/synchronizable.h>
#include <threadsafe/vocabulary.h>
