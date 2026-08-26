#include <threadsafe/threadsafe.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {
class thread_affine_context {
public:
    thread_affine_context();
    ~thread_affine_context();
    void draw();
private:
    mutable std::mutex gate_;
    std::thread::id owner_;
    void* native_handle_;
};
struct HoldsContext { thread_affine_context ctx; };
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(thread_affine_context);
// The user does the extra work the library never asked for:
template <> struct threadsafe::is_sendable<thread_affine_context> : std::false_type {};

using threadsafe::is_sendable_v;
static_assert(!is_sendable_v<thread_affine_context>);
static_assert(!is_sendable_v<HoldsContext>, "aggregates now respect the opt-out");
static_assert(!is_sendable_v<std::vector<thread_affine_context>>);

// but every indirection ignores it, because they all route to is_synchronizable:
static_assert(is_sendable_v<thread_affine_context*>, "raw pointer ignores the opt-out");
static_assert(is_sendable_v<thread_affine_context&>, "reference ignores the opt-out");
static_assert(is_sendable_v<thread_affine_context&&>, "rvalue reference ignores it too");
static_assert(is_sendable_v<std::shared_ptr<thread_affine_context>>,
              "HAZARD: the receiving thread may drop the last reference and "
              "destroy the context there");
static_assert(is_sendable_v<std::unique_ptr<thread_affine_context>> == false,
              "unique_ptr does route through is_sendable, so it obeys");
