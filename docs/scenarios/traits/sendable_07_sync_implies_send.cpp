#include <threadsafe/threadsafe.h>
#include <mutex>
#include <thread>

namespace {

// The Rust MutexGuard shape: every access is serialized by the mutex, so the
// object really is usable from several threads at once (Sync). But the handle it
// owns is registered with the creating thread and pthread/GL/COM require it to
// be released there, so it must NOT be moved to another thread (!Send).
class thread_affine_context {
public:
    thread_affine_context();
    ~thread_affine_context();          // must run on owner_
    void draw();                        // takes gate_ internally

private:
    mutable std::mutex gate_;
    std::thread::id owner_;
    void* native_handle_;
};

}

// The user's claim: "several threads may use it at once". True.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(thread_affine_context);

using threadsafe::is_sendable_v;

static_assert(threadsafe::is_synchronizable_v<thread_affine_context>);

// Nobody said it may be MOVED to another thread -- yet:
static_assert(is_sendable_v<thread_affine_context>,
              "Sync silently implies Send: the user-provided destructor is not "
              "even looked at");

static_assert(threadsafe::launchable_task<
                  decltype([](thread_affine_context) {}), thread_affine_context>
                  == false,
              "only move-constructibility stops it here");

// ... and every indirection to it is sendable too, which is how it escapes:
static_assert(is_sendable_v<thread_affine_context*>);
static_assert(is_sendable_v<std::shared_ptr<thread_affine_context>>,
              "the last shared owner on the other thread runs ~ctx there");

namespace {
struct HoldsContext { thread_affine_context ctx; };
}
static_assert(is_sendable_v<HoldsContext>,
              "and it propagates into aggregates");
