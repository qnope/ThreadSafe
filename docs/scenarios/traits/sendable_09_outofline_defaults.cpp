#include <threadsafe/threadsafe.h>
#include <meta>

namespace {

struct InClassDefaulted {
    int v;
    ~InClassDefaulted() = default;
    InClassDefaulted(const InClassDefaulted&) = default;
};

struct OutOfLineDefaulted {
    int v;
    ~OutOfLineDefaulted();
    OutOfLineDefaulted(const OutOfLineDefaulted&);
};

}

// Asked BEFORE the out-of-line "= default" is seen.
constexpr bool before_send = threadsafe::is_sendable_v<OutOfLineDefaulted>;

namespace {
OutOfLineDefaulted::~OutOfLineDefaulted() = default;
OutOfLineDefaulted::OutOfLineDefaulted(const OutOfLineDefaulted&) = default;
}

static_assert(threadsafe::is_sendable_v<InClassDefaulted>);
static_assert(!before_send, "before the out-of-line default: user-written");

// Asked AFTER. is_sendable_v<T> is a variable template: one instantiation per
// TU, so this reads the cached first answer -- but the *info-level* face and a
// fresh consteval call re-evaluate.
static_assert(!threadsafe::is_sendable_v<OutOfLineDefaulted>);
static_assert(!threadsafe::detail::default_is_sendable(^^OutOfLineDefaulted),
              "does the reflective walk change its mind after the definition?");
