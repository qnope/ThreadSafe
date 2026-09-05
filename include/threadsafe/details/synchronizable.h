#pragma once

#include <atomic>
#include <type_traits>

#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable_base.h>

namespace threadsafe {

template <class F>
    requires std::is_function_v<F>
struct is_unsafe_synchronizable<F> : std::true_type {};

template <class T>
struct is_unsafe_synchronizable<std::atomic<T>>
    : std::bool_constant<is_sendable_v<T>> {};

}
