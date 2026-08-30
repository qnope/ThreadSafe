#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_sendable<std::default_delete<T>> {
    static constexpr TraitAnswer value = {};
};

template <class T, class D>
struct is_sendable<std::unique_ptr<T, D>> {
    using pointee = std::remove_all_extents_t<T>;

    static constexpr TraitAnswer value = []{
        if (const auto value = is_sendable_v<pointee>; !value)
            return value;

        if (const auto value = is_sendable_v<D>; !value)
            return value;

        return detail::dynamic_type_is_known(^^pointee);
    }();
};


template <class T, class D>
struct is_lifetime_aware<std::unique_ptr<T, D>> {
    using pointee = std::remove_all_extents_t<T>;

    static constexpr TraitAnswer value = []{
        if (const auto value = is_lifetime_aware_v<pointee>; !value)
            return value;

        if (const auto value = is_lifetime_aware_v<D>; !value)
            return value;

        return detail::dynamic_type_is_known(^^pointee);
    }();
};

template <class T>
struct is_sendable<std::shared_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_sendable<std::weak_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_sendable<std::reference_wrapper<T>>
    : is_synchronizable<std::remove_cv_t<T>> {};

template <class T>
struct is_synchronizable<const std::default_delete<T>> {
    static constexpr TraitAnswer value = {};
};

// The one indirection that trusts the pointee's const: unique ownership means no
// other alias can write through it. That trust needs the dynamic type, exactly as
// the is_sendable rule above does -- a derived object may add a mutable member the
// walk never saw.
template <class T, class D>
struct is_synchronizable<const std::unique_ptr<T, D>>
{
    using pointee = std::remove_all_extents_t<T>;

    static constexpr TraitAnswer value = []{
        if (const auto value = is_synchronizable_v<pointee>; !value)
            return value;

        if (const auto value = is_synchronizable_v<const D>; !value)
            return value;

        return detail::dynamic_type_is_known(^^pointee);
    }();
};

template <class T>
struct is_synchronizable<const std::shared_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::weak_ptr<T>>
    : is_synchronizable<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

template <class T>
struct is_synchronizable<const std::reference_wrapper<T>>
    : is_synchronizable<std::remove_cv_t<T>> {};

}
