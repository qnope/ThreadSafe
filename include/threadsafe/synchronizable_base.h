#pragma once

namespace threadsafe {

template <class T>
constexpr bool is_synchronizable = false;

}

#define THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(...)         \
    template <>                                              \
    inline constexpr bool ::threadsafe::is_synchronizable<__VA_ARGS__> = true
