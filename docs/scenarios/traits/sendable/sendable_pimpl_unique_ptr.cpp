#include <threadsafe/threadsafe.h>
#include <memory>

// The pimpl idiom: the trait's own diagnostic tells the user to specialize
// is_sendable for "types holding a pointer to an incomplete type (the pimpl
// idiom)", so asking the question must at least be possible.
struct Impl;

struct Widget {
    std::unique_ptr<Impl> impl;
};

static_assert(!threadsafe::is_sendable_v<Widget>);

int main() {}
