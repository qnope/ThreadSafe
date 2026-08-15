#pragma once

#include <atomic>
#include <type_traits>

#include <threadsafe/sendable.h>
#include <threadsafe/synchronizable_base.h>

namespace threadsafe {

template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
constexpr bool is_synchronizable<F> = true;

template <class T>
constexpr bool is_synchronizable<std::atomic<T>> = is_sendable<T>;

}
