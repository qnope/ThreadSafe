#pragma once

#include <concepts>
#include <meta>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

// The launcher takes its callable and its arguments by value: owning them is
// what lets it hand them to a thread, and owning them means moving them in.
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

// Replays the concept one assertion at a time, in reading order — the callable,
// then the arguments — so the first failing trait names the culprit. The final
// throw is unreachable while the concept and this function agree; it keeps the
// fallback honest should they ever drift apart.
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
    assert_sendable<F>();
    assert_lifetime_aware<F>();
    (assert_sendable<Args>(), ...);
    (assert_lifetime_aware<Args>(), ...);

    throw std::meta::exception(
        u8"launch_task rejects this call but every trait holds", ^^F);
}

template <class F, class... Args>
consteval void explain_launch_scoped_task() {
    assert_ownable_by_launcher<F, Args...>();
    assert_sendable<F>();
    (assert_sendable<Args>(), ...);

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

    // Fallback: the constrained overload is more constrained, so it always wins
    // when it applies. This one is instantiated only on a rejection, and exists
    // only to name it — instantiating it is always a compile error.
    template <typename F, typename... Args>
    void launch_task(F, Args...) {
        detail::explain_launch_task<F, Args...>();
    }

    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it does
    // not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
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
