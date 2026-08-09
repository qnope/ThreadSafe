#pragma once

#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/safe_callable.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

// Launches tasks on std::jthread, with the traits enforced at the call site.
// Constraints apply to the decayed types because that is what std::jthread
// copies into the new thread; std::ref(x) decays to std::reference_wrapper<X>,
// so the reference_wrapper trait specializations apply automatically.
//
// The launcher itself is deliberately not synchronizable: threads_ is a plain
// vector, so launching from two threads at once would race. is_synchronizable
// defaults to false, which is the correct answer here.
class asynchronous_task_launcher {
    // std::jthread prepends a std::stop_token whenever the callable accepts
    // one. That argument is injected behind the constraints below, so the
    // traits must hold for it unconditionally rather than by inspection.
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");

public:
    // Stores the jthread; it is joined when the launcher is destroyed
    // (jthread RAII). The task may outlive the calling scope, so F and Args
    // must keep their data alive: lifetime_aware in addition to sendable
    // (≈ Rust's 'static bound on thread::spawn).
    //
    // F is checked for sendable as well as safe_callable: jthread decay-copies
    // the callable into the new thread and destroys that copy *on* that
    // thread, which is exactly what is_sendable governs. safe_callable does
    // not imply it — an empty class with a user-provided copy constructor is a
    // safe callable and not sendable.
    template <typename F, typename... Args>
        requires safe_callable<std::decay_t<F>>
              && sendable<std::decay_t<F>>
              && lifetime_aware<std::decay_t<F>>
              && (sendable<std::decay_t<Args>> && ...)
              && (lifetime_aware<std::decay_t<Args>> && ...)
    void launch_task(F&& f, Args&&... args) {
        threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...);
    }

    // Spawns one jthread and waits for it to terminate before returning.
    // The caller provably outlives the task, so lifetime_aware is not
    // required: references into the calling scope may cross via std::ref,
    // and sendable already demands their referent be synchronizable.
    //
    // Note that join() proves the task has finished, not that it published
    // nothing: a task handed both a caller-scope reference and a longer-lived
    // object can store the former into the latter. No type-level trait can
    // see that, and it is the one dangling shape this API cannot exclude.
    template <typename F, typename... Args>
        requires safe_callable<std::decay_t<F>>
              && sendable<std::decay_t<F>>
              && (sendable<std::decay_t<Args>> && ...)
    void launch_scoped_task(F&& f, Args&&... args) {
        std::jthread task{std::forward<F>(f), std::forward<Args>(args)...};
        task.join();
    }

private:
    std::vector<std::jthread> threads_;
};

}  // namespace threadsafe
