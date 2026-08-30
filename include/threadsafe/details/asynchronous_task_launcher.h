#pragma once

#include <concepts>
#include <meta>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class F, class... Args>
concept ownable_by_launcher =
    std::move_constructible<F> && (std::move_constructible<Args> && ...);

template <class F, class... Args>
concept launchable_task = ownable_by_launcher<F, Args...>
                       && sendable<F>
                       && lifetime_aware<F>
                       && (sendable<Args> && ...)
                       && (lifetime_aware<Args> && ...);

template <class F, class... Args>
concept launchable_scoped_task = ownable_by_launcher<F, Args...>
                              && sendable<F>
                              && (sendable<Args> && ...);

namespace detail {

inline consteval void require(TraitAnswer answer, std::string_view trait,
                              std::meta::info type) {
    if (answer)
        return;

    std::string explanation(trait);
    if (!answer.paths().empty())
        explanation += " at " + std::string(answer.full_path());
    explanation += ": " + std::string(answer.error_message);

    throw std::meta::exception(explanation, type);
}

template <class F, class... Args>
consteval void assert_ownable_by_launcher() {
    if (!std::move_constructible<F>)
        throw std::meta::exception(
            u8"the launcher owns its callable, so a non-movable one cannot "
            u8"cross; share it with std::ref instead",
            ^^F);

    (..., [] {
        if (!std::move_constructible<Args>)
            throw std::meta::exception(
                u8"the launcher owns its arguments, so a non-movable one "
                u8"cannot cross; share it with std::ref instead",
                ^^Args);
    }());
}

template <class F, class... Args>
consteval void explain_launch_task() {
    assert_ownable_by_launcher<F, Args...>();
    require(is_sendable_v<F>, "is_sendable", ^^F);
    require(is_lifetime_aware_v<F>, "is_lifetime_aware", ^^F);
    (require(is_sendable_v<Args>, "is_sendable", ^^Args), ...);
    (require(is_lifetime_aware_v<Args>, "is_lifetime_aware", ^^Args), ...);

    throw std::meta::exception(
        u8"launch_task rejects this call but every trait holds", ^^F);
}

template <class F, class... Args>
consteval void explain_launch_scoped_task() {
    assert_ownable_by_launcher<F, Args...>();
    require(is_sendable_v<F>, "is_sendable", ^^F);
    (require(is_sendable_v<Args>, "is_sendable", ^^Args), ...);

    throw std::meta::exception(
        u8"launch_scoped_task rejects this call but every trait holds", ^^F);
}

}

class asynchronous_task_launcher {
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");

public:
    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    void launch_task(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }

    template <typename F, typename... Args>
    void launch_task(F, Args...) {
        detail::explain_launch_task<F, Args...>();
    }

    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }

    template <typename F, typename... Args>
    void launch_scoped_task(F, Args...) {
        detail::explain_launch_scoped_task<F, Args...>();
    }

private:
    std::vector<std::jthread> threads_;
};

}
