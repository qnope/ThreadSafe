#pragma once

#include <atomic>
#include <type_traits>

#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable_base.h>

namespace threadsafe {

template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
struct is_unsafe_synchronizable<F> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T>
struct is_unsafe_synchronizable<std::atomic<T>> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<T>.prepend_path(detail::pointee_step);
    }
};

}
