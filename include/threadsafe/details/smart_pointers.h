#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

namespace detail {

template <class Pointee>
inline consteval TraitAnswer unique_ptr_answer(TraitAnswer pointee_answer,
                                               TraitAnswer deleter_answer) {
    if (!pointee_answer)
        return pointee_answer.prepend_path(pointee_step);

    if (!deleter_answer)
        return deleter_answer.prepend_path("deleter");

    return dynamic_type_is_known<Pointee>;
}

}

template <class T>
struct is_unsafe_sendable<std::default_delete<T>> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T, class D>
struct is_unsafe_sendable<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;
        return detail::unique_ptr_answer<pointee>(is_sendable_v<pointee>,
                                                  is_sendable_v<D>);
    }
};

template <class T, class D>
struct is_unsafe_lifetime_aware<std::unique_ptr<T, D>> {
    static consteval TraitAnswer diagnose() {
        using pointee = std::remove_all_extents_t<T>;
        return detail::unique_ptr_answer<pointee>(is_lifetime_aware_v<pointee>,
                                                  is_lifetime_aware_v<D>);
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
        return detail::unique_ptr_answer<pointee>(is_synchronizable_v<pointee>,
                                                  is_synchronizable_v<const D>);
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
