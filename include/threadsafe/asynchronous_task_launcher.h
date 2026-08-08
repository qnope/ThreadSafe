#pragma once

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
class asynchronous_task_launcher {
public:
    // Stores the jthread; it is joined when the launcher is destroyed
    // (jthread RAII). The task may outlive the calling scope, so F and Args
    // must keep their data alive: lifetime_aware in addition to sendable
    // (≈ Rust's 'static bound on thread::spawn).
    template <typename F, typename... Args>
        requires safe_callable<std::decay_t<F>>
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
    template <typename F, typename... Args>
        requires safe_callable<std::decay_t<F>>
              && (sendable<std::decay_t<Args>> && ...)
    void launch_scoped_task(F&& f, Args&&... args) {
        std::jthread task{std::forward<F>(f), std::forward<Args>(args)...};
        task.join();
    }

private:
    std::vector<std::jthread> threads_;
};

}  // namespace threadsafe
