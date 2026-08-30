#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

template <class T>
struct is_unsafe_sendable<std::default_delete<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T, class D>
struct is_unsafe_sendable<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;

        if (const auto pointee_answer = is_sendable_v<pointee>; !pointee_answer)
            return pointee_answer.prepend_path(detail::pointee_step);

        if (const auto deleter_answer = is_sendable_v<D>; !deleter_answer)
            return deleter_answer.prepend_path("deleter");

        return detail::dynamic_type_is_known<pointee>;
    }
};

template <class T, class D>
struct is_unsafe_lifetime_aware<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;

        if (const auto pointee_answer = is_lifetime_aware_v<pointee>;
            !pointee_answer)
            return pointee_answer.prepend_path(detail::pointee_step);

        if (const auto deleter_answer = is_lifetime_aware_v<D>; !deleter_answer)
            return deleter_answer.prepend_path("deleter");

        return detail::dynamic_type_is_known<pointee>;
    }
};

template <class T>
struct is_unsafe_sendable<std::shared_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<
            std::remove_cv_t<std::remove_all_extents_t<T>>>
            .prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_sendable<std::weak_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<
            std::remove_cv_t<std::remove_all_extents_t<T>>>
            .prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_sendable<std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>.prepend_path(
            detail::referent_step);
    }
};

template <class T>
struct is_unsafe_synchronizable<const std::default_delete<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T, class D>
struct is_unsafe_synchronizable<const std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;

        if (const auto pointee_answer = is_synchronizable_v<pointee>;
            !pointee_answer)
            return pointee_answer.prepend_path(detail::pointee_step);

        if (const auto deleter_answer = is_synchronizable_v<const D>;
            !deleter_answer)
            return deleter_answer.prepend_path("deleter");

        return detail::dynamic_type_is_known<pointee>;
    }
};

template <class T>
struct is_unsafe_synchronizable<const std::shared_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<
            std::remove_cv_t<std::remove_all_extents_t<T>>>
            .prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_synchronizable<const std::weak_ptr<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<
            std::remove_cv_t<std::remove_all_extents_t<T>>>
            .prepend_path(detail::pointee_step);
    }
};

template <class T>
struct is_unsafe_synchronizable<const std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>.prepend_path(
            detail::referent_step);
    }
};

}
