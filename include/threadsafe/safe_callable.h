#pragma once

#include <type_traits>

#include <threadsafe/synchronizable.h>

namespace threadsafe {

namespace detail {
template <class F>
consteval bool default_is_safe_callable();
}

template <class F>
constexpr bool is_safe_callable = detail::default_is_safe_callable<F>();

template <function_type F>
constexpr bool is_safe_callable<F*> = true;

template <class F>
concept safe_callable = is_safe_callable<F>;

namespace detail {

template <class F>
consteval bool default_is_safe_callable() {
    if constexpr (!std::is_same_v<F, std::remove_cv_t<F>>) {
        return is_safe_callable<std::remove_cv_t<F>>;
    } else {
        return is_synchronizable<F> || (std::is_class_v<F> && std::is_empty_v<F>);
    }
}

}

}
